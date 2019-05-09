#include "bytecode.hpp"
#include "vcode.h"
#include "phase.h"

#include <gtest/gtest.h>
#include <map>

#include "test_util.h"

#define __ asm_.

using namespace testing;

namespace {
   struct CheckBytecode {
      CheckBytecode(uint16_t v) : value(v) {}

      static const uint16_t DONT_CARE = 0xffff;
      static const uint16_t REG_MASK  = 0x0100;

      uint16_t value;
   };

}

static const CheckBytecode _(CheckBytecode::DONT_CARE);
static const CheckBytecode _1(CheckBytecode::REG_MASK | 1);
static const CheckBytecode _2(CheckBytecode::REG_MASK | 2);
static const CheckBytecode _3(CheckBytecode::REG_MASK | 3);
static const CheckBytecode _4(CheckBytecode::REG_MASK | 4);

static vcode_unit_t context = nullptr;
static vcode_type_t i32_type = VCODE_INVALID_TYPE;

static void setup(void)
{
   context = emit_context(ident_new("unit_test"));
   i32_type = vtype_int(INT32_MIN, INT32_MAX);
}

static void teardown(void)
{
   vcode_unit_unref(context);
   context = nullptr;
   i32_type = VCODE_INVALID_TYPE;
}

class BytecodeTest : public ::testing::Test {
protected:
   BytecodeTest()
      : asm_(InterpMachine::get())
   {}

   using OpCode = Bytecode::OpCode;

   virtual void SetUp() override;
   virtual void TearDown() override;

   static void SetUpTestCase();
   static void TearDownTestCase();

   void check_bytecodes(const Bytecode *b,
                        const std::vector<CheckBytecode>&& expect);

   Bytecode::Assembler asm_;
   vcode_unit_t        context_ = nullptr;
   vcode_type_t        i32_type_ = VCODE_INVALID_TYPE;

private:
   static lib_t work_;
};

lib_t BytecodeTest::work_(nullptr);

void BytecodeTest::SetUp()
{
   context_ = emit_context(ident_new("gtest"));
   i32_type_ = vtype_int(INT32_MIN, INT32_MAX);
}

void BytecodeTest::TearDown()
{
   vcode_unit_unref(context_);
   context_ = nullptr;
}

void BytecodeTest::SetUpTestCase()
{
   work_ = lib_tmp("gtest");
   lib_set_work(work_);

   input_from_file(TESTDIR "/bytecode/functions.vhd");

   tree_t pack = parse();
   ASSERT_NE(nullptr, pack);
   EXPECT_EQ(T_PACKAGE, tree_kind(pack));
   EXPECT_TRUE(sem_check(pack));

   tree_t body = parse();
   ASSERT_NE(nullptr, body);
   EXPECT_EQ(T_PACK_BODY, tree_kind(body));
   EXPECT_TRUE(sem_check(body));

   simplify(body, (eval_flags_t)0);
   lower_unit(body);

   EXPECT_EQ(nullptr, parse());
   EXPECT_EQ(0, parse_errors());
   EXPECT_EQ(0, sem_errors());
}

void BytecodeTest::TearDownTestCase()
{
   lib_set_work(nullptr);
   lib_free(work_);
   work_ = nullptr;
}

void BytecodeTest::check_bytecodes(const Bytecode *b,
                                   const std::vector<CheckBytecode>&& expect)
{
   const uint8_t *p = b->bytes();
   std::map<int, int> match;

   for (const CheckBytecode& c : expect) {
      if (p >= b->bytes() + b->length()) {
         FAIL() << "expected more than " << b->length() << " bytecodes";
         return;
      }
      else if ((c.value & 0xff00) == 0) {
         // Directly compare the bytecode
         EXPECT_EQ(c.value, *p) << "bytecode mismatch at offset "
                                << (p - b->bytes())
                                << std::endl << std::endl << *b;
         if (c.value != *p)
            return;
         ++p;
      }
      else if (c.value == CheckBytecode::DONT_CARE)
         ++p;
      else if ((c.value & CheckBytecode::REG_MASK) == CheckBytecode::REG_MASK) {
         const int num = c.value & 0xff;
         if (match.find(num) == match.end())
            match[num] = *p;
         else {
            EXPECT_EQ(match[num], *p) << "placeholder _" << num
                                      << " mismatch at offset"
                                      << (p - b->bytes());
            if (match[num] != *p)
               return;
         }
         ++p;
      }
      else {
         FAIL() << "unexpected bytecode check" << c.value;
         return;
      }
   }

   EXPECT_EQ(b->length(), p - b->bytes()) << "did not match all bytecodes"
                                          << std::endl << std::endl << *b;
}

void check_bytecodes(const Bytecode *b,
                     const std::vector<CheckBytecode>&& expect)
{
   const uint8_t *p = b->bytes();
   std::map<int, int> match;

   for (const CheckBytecode& c : expect) {
      if (p >= b->bytes() + b->length()) {
         fail("expected more than %d bytecodes", b->length());
         return;
      }
      else if ((c.value & 0xff00) == 0) {
         // Directly compare the bytecode
         if (c.value != *p) {
            BufferPrinter printer;
            b->dump(printer);
            fail("bytecode mismatch at offset %d\n\n%s",
                 p - b->bytes(), printer.buffer());
         }
         else if (c.value != *p)
            return;
         ++p;
      }
      else if (c.value == CheckBytecode::DONT_CARE)
         ++p;
      else if ((c.value & CheckBytecode::REG_MASK) == CheckBytecode::REG_MASK) {
         const int num = c.value & 0xff;
         if (match.find(num) == match.end())
            match[num] = *p;
         else {
            if (match[num] != *p) {
               BufferPrinter printer;
               b->dump(printer);
               fail("placeholder _%d mismatch at offset %d\n\n%s",
                    num, p - b->bytes(), printer.buffer());
            }
            else if (match[num] != *p)
               return;
         }
         ++p;
      }
      else {
         fail("unexpected bytecode check %x", c.value);
         return;
      }
   }

   if ((int)b->length() != p - b->bytes()) {
      BufferPrinter printer;
      b->dump(printer);
      fail("did not match all bytecodes\n\n%s", printer.buffer());
   }
}

TEST_F(BytecodeTest, compile_add1) {
   vcode_unit_t unit = emit_function(ident_new("add1"), context_, i32_type_);

   vcode_reg_t p0 = emit_param(i32_type_, i32_type_, ident_new("x"));
   emit_return(emit_add(p0, emit_const(i32_type_, 1)));

   vcode_opt();

   Bytecode *b = compile(InterpMachine::get(), unit);
   ASSERT_NE(nullptr, b);

   check_bytecodes(b, {
         Bytecode::ADDB, 0, 0x01,
         Bytecode::RET
      });

   vcode_unit_unref(unit);
}

TEST_F(BytecodeTest, select) {
   vcode_unit_t unit = emit_function(ident_new("max"), context_, i32_type_);

   vcode_reg_t p0 = emit_param(i32_type_, i32_type_, ident_new("x"));
   vcode_reg_t p1 = emit_param(i32_type_, i32_type_, ident_new("y"));
   vcode_reg_t cmp = emit_cmp(VCODE_CMP_GT, p0, p1);
   emit_return(emit_select(cmp, p0, p1));

   vcode_opt();

   Bytecode *b = compile(InterpMachine::get(), unit);
   ASSERT_NE(nullptr, b);

   check_bytecodes(b, {
         Bytecode::CMP, _1, _2,
         Bytecode::MOV, _, _1,
         Bytecode::JMPC, Bytecode::GT, _, _,
         Bytecode::MOV, _, _2,
         Bytecode::MOV, 0, _,
         Bytecode::RET
      });

   vcode_unit_unref(unit);
}

TEST_F(BytecodeTest, patch) {
   Bytecode::Label L1;

   __ bind(L1);
   __ jmp(L1);
   __ jmp(L1);
   __ cbnz(Bytecode::R(0), L1);
   __ cbnz(Bytecode::R(0), L1);

   __ patch_branch(3, 0);
   __ patch_branch(6, 10);

   Bytecode *b = __ finish();

   check_bytecodes(b, {
         Bytecode::JMP, 0xff, 0xff,
         Bytecode::JMP, 0xfc, 0xff,
         Bytecode::CBNZ, 0, 0x02, 0x00,
         Bytecode::CBNZ, 0, 0xf4, 0xff,
      });
}

TEST_F(BytecodeTest, compile_fact) {
   vcode_unit_t unit = vcode_find_unit(ident_new("GTEST.FUNCTIONS.FACT(I)I"));
   ASSERT_NE(nullptr, unit);

   vcode_select_unit(unit);

   Bytecode *b = compile(InterpMachine::get(), unit);
   ASSERT_NE(nullptr, b);

   EXPECT_EQ(32, b->frame_size());

   check_bytecodes(b, {
         Bytecode::MOVB, _, 1,
         Bytecode::STR, _, _, _, _,
         Bytecode::CMP, _, _,
         Bytecode::STR, _, _, _, _,
         Bytecode::JMPC, Bytecode::GT, _, _,
         Bytecode::JMP, _, _,
         Bytecode::MOVB, _, 1,
         Bytecode::STR, _, _, _, _,
         Bytecode::JMP, _, _,
         Bytecode::LDR, _, _, _, _,
         Bytecode::RET,
         Bytecode::LDR, _, _, _, _,
         Bytecode::LDR, _, _, _, _,
         Bytecode::MUL, _, _,
         Bytecode::STR, _, _, _, _,
         Bytecode::MOV, _, _,
         Bytecode::ADDB, _, 1,
         Bytecode::STR, _, _, _, _,
         Bytecode::LDR, _, _, _, _,
         Bytecode::CMP, _, _,
         Bytecode::JMPC, Bytecode::Z, _, _,
         Bytecode::JMP, _, _
      });
}

TEST_F(BytecodeTest, compile_sum) {
   vcode_unit_t unit = vcode_find_unit(
      ident_new("GTEST.FUNCTIONS.SUM(25GTEST.FUNCTIONS.INT_ARRAY)I"));
   ASSERT_NE(nullptr, unit);

   Bytecode *b = compile(InterpMachine::get(), unit);
   ASSERT_NE(nullptr, b);

   vcode_dump();
   b->dump();
}

TEST_F(BytecodeTest, add_sub_reuse) {
   vcode_unit_t unit = emit_function(ident_new("add_sub_reuse"),
                                     context_, i32_type_);

   vcode_reg_t p0 = emit_param(i32_type_, i32_type_, ident_new("x"));
   vcode_reg_t p1 = emit_param(i32_type_, i32_type_, ident_new("x"));

   vcode_reg_t t0 = emit_add(p0, p1);
   vcode_reg_t t1 = emit_sub(t0, p1);
   vcode_reg_t t2 = emit_add(t1, p0);
   emit_return(t2);

   vcode_opt();

   Bytecode *b = compile(InterpMachine::get(), unit);
   ASSERT_NE(nullptr, b);

   check_bytecodes(b, {
         Bytecode::MOV, _1, 0,
         Bytecode::ADD, _1, 1,
         Bytecode::ADD, _1, 1,
         Bytecode::ADD, _1, 0,
         Bytecode::MOV, 0, _1,
         Bytecode::RET
      });

   vcode_unit_unref(unit);
}

TEST_F(BytecodeTest, unwrap) {
   vcode_type_t pi32_type = vtype_pointer(i32_type_);
   vcode_unit_t unit = emit_function(ident_new("unwrap"),
                                     context_, pi32_type);

   vcode_type_t ui32_type = vtype_uarray(1, i32_type_, i32_type_);
   vcode_reg_t p0 = emit_param(ui32_type, ui32_type, ident_new("p0"));

   emit_return(emit_unwrap(p0));

   vcode_opt();

   Bytecode *b = compile(InterpMachine::get(), unit);
   ASSERT_NE(nullptr, b);

   check_bytecodes(b, {
         Bytecode::LDR, 0, InterpMachine::SP_REG, 0, 0,
         Bytecode::RET
      });

   vcode_unit_unref(unit);
}

TEST_F(BytecodeTest, uarray_dir) {
   vcode_unit_t unit = emit_function(ident_new("uarray_dir"),
                                     context_, i32_type_);

   vcode_type_t ui32_type = vtype_uarray(1, i32_type_, i32_type_);
   vcode_reg_t p0 = emit_param(ui32_type, ui32_type, ident_new("p0"));

   emit_return(emit_cast(i32_type_, i32_type_, emit_uarray_dir(p0, 0)));

   vcode_opt();

   Bytecode *b = compile(InterpMachine::get(), unit);
   ASSERT_NE(nullptr, b);

   check_bytecodes(b, {
         Bytecode::LDR, 0, InterpMachine::SP_REG, 4, 0,
         Bytecode::RET
      });

   vcode_unit_unref(unit);
}

START_TEST(test_uarray_dir_highdim)
{
   vcode_unit_t unit = emit_function(ident_new("uarray_dir"),
                                     context, i32_type);

   vcode_type_t ui32_type = vtype_uarray(10, i32_type, i32_type);
   vcode_reg_t p0 = emit_param(ui32_type, ui32_type, ident_new("p0"));

   emit_return(emit_cast(i32_type, i32_type, emit_uarray_dir(p0, 9)));

   vcode_opt();

   Bytecode *b = compile(InterpMachine::get(), unit);
   ASSERT_NE(nullptr, b);

   check_bytecodes(b, {
         Bytecode::LDR, 0, InterpMachine::SP_REG, 4, 0,
         Bytecode::ANDW, 0, 0, 2, 0, 0,
         Bytecode::RET
      });

   vcode_unit_unref(unit);
}
END_TEST

extern "C" Suite *get_bytecode_tests(void)
{
   Suite *s = suite_create("bytecode");

   TCase *tc = nvc_unit_test();
   tcase_add_checked_fixture(tc, setup, teardown);
   tcase_add_test(tc, test_uarray_dir_highdim);
   suite_add_tcase(s, tc);

   return s;
}

extern "C" int run_gtests(int argc, char **argv)
{
   InitGoogleTest(&argc, argv);
   return RUN_ALL_TESTS();
}
