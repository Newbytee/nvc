//
//  Copyright (C) 2019  Nick Gasson
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

#pragma once

#include "bytecode.hpp"

#include <map>

class Compiler {
public:
   explicit Compiler(const Machine &m);
   Compiler(const Compiler &) = delete;

   Bytecode *compile(vcode_unit_t unit);

private:
   void compile_const(int op);
   void compile_addi(int op);
   void compile_return(int op);
   void compile_store(int op);
   void compile_cmp(int op);
   void compile_load(int op);
   void compile_jump(int op);
   void compile_cond(int op);
   void compile_mul(int op);
   void compile_uarray_left(int op);
   void compile_uarray_right(int op);
   void compile_uarray_dir(int op);
   void compile_unwrap(int op);
   void compile_cast(int op);
   void compile_range_null(int op);
   void compile_select(int op);

   Bytecode::Label &label_for_block(vcode_block_t block);

   struct Mapping {
      enum Kind { REGISTER, STACK };

      Kind kind;
      union {
         Bytecode::Register reg;
         int slot;
      };
   };

   struct Patch {
      vcode_block_t block;
      Bytecode::Label label;
   };

   const Mapping &map_vcode_reg(vcode_reg_t reg) const;
   const Mapping &map_vcode_var(vcode_reg_t reg) const;

   const Machine machine_;
   Bytecode::Assembler asm_;
   std::map<vcode_var_t, Mapping> var_map_;
   std::vector<Mapping> reg_map_;
   std::vector<Bytecode::Label> block_map_;
};