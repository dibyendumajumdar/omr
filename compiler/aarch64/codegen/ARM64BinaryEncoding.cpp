/*******************************************************************************
 * Copyright (c) 2018, 2019 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at http://eclipse.org/legal/epl-2.0
 * or the Apache License, Version 2.0 which accompanies this distribution
 * and is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following Secondary
 * Licenses when the conditions for such availability set forth in the
 * Eclipse Public License, v. 2.0 are satisfied: GNU General Public License,
 * version 2 with the GNU Classpath Exception [1] and GNU General Public
 * License, version 2 with the OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include <stddef.h>
#include <stdint.h>

#include "codegen/ARM64ConditionCode.hpp"
#include "codegen/ARM64Instruction.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/InstructionDelegate.hpp"
#include "codegen/Linkage.hpp"
#include "codegen/Relocation.hpp"
#include "runtime/CodeCacheManager.hpp"

uint8_t *OMR::ARM64::Instruction::generateBinaryEncoding()
   {
   uint8_t *instructionStart = cg()->getBinaryBufferCursor();
   uint8_t *cursor = instructionStart;
   cursor = getOpCode().copyBinaryToBuffer(instructionStart);
   cursor += ARM64_INSTRUCTION_LENGTH;
   setBinaryLength(ARM64_INSTRUCTION_LENGTH);
   setBinaryEncoding(instructionStart);
   return cursor;
   }

int32_t OMR::ARM64::Instruction::estimateBinaryLength(int32_t currentEstimate)
   {
   setEstimatedBinaryLength(ARM64_INSTRUCTION_LENGTH);
   return currentEstimate + self()->getEstimatedBinaryLength();
   }

uint8_t *TR::ARM64ImmInstruction::generateBinaryEncoding()
   {
   uint8_t *instructionStart = cg()->getBinaryBufferCursor();
   uint8_t *cursor = instructionStart;
   cursor = getOpCode().copyBinaryToBuffer(instructionStart);
   insertImmediateField(toARM64Cursor(cursor));
   cursor += ARM64_INSTRUCTION_LENGTH;
   setBinaryLength(ARM64_INSTRUCTION_LENGTH);
   setBinaryEncoding(instructionStart);
   return cursor;
   }

uint8_t *TR::ARM64ImmSymInstruction::generateBinaryEncoding()
   {
   uint8_t *instructionStart = cg()->getBinaryBufferCursor();
   uint8_t *cursor = instructionStart;
   cursor = getOpCode().copyBinaryToBuffer(instructionStart);

   if (getOpCodeValue() == TR::InstOpCode::bl)
      {
      TR::SymbolReference *symRef = getSymbolReference();
      TR::LabelSymbol *label = symRef->getSymbol()->getLabelSymbol();

      TR::ResolvedMethodSymbol *sym = symRef->getSymbol()->getResolvedMethodSymbol();

      if (cg()->comp()->isRecursiveMethodTarget(sym))
         {
         intptrj_t jitToJitStart = cg()->getLinkage()->entryPointFromCompiledMethod();

         TR_ASSERT_FATAL(jitToJitStart, "Unknown compiled method entry point.  Entry point should be available by now.");

         TR_ASSERT_FATAL(TR::Compiler->target.cpu.isTargetWithinUnconditionalBranchImmediateRange(jitToJitStart, (intptrj_t)cursor),
                         "Target address is out of range");

         intptrj_t distance = jitToJitStart - (intptrj_t)cursor;
         insertImmediateField(toARM64Cursor(cursor), distance);
         setAddrImmediate(jitToJitStart);
         }
      else if (label != NULL)
         {
         cg()->addRelocation(new (cg()->trHeapMemory()) TR::LabelRelative32BitRelocation(cursor, label));
         TR::InstructionDelegate::encodeBranchToLabel(cg(), this, cursor);
         }
      else
         {
         TR::MethodSymbol *method = symRef->getSymbol()->getMethodSymbol();
         if (method && method->isHelper())
            {
            intptrj_t destination = (intptrj_t)symRef->getMethodAddress();

            if (cg()->directCallRequiresTrampoline(destination, (intptrj_t)cursor))
               {
               destination = TR::CodeCacheManager::instance()->findHelperTrampoline(symRef->getReferenceNumber(), (void *)cursor);

               TR_ASSERT_FATAL(TR::Compiler->target.cpu.isTargetWithinUnconditionalBranchImmediateRange(destination, (intptrj_t)cursor),
                               "Target address is out of range");
               }

            intptrj_t distance = destination - (intptrj_t)cursor;
            insertImmediateField(toARM64Cursor(cursor), distance);
            setAddrImmediate(destination);

            cg()->addExternalRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(
                                           cursor,
                                           (uint8_t *)symRef,
                                           TR_HelperAddress, cg()),
                                        __FILE__, __LINE__, getNode());
            }
         else
            {
            intptrj_t destination = getAddrImmediate();

            if (cg()->directCallRequiresTrampoline(destination, (intptrj_t)cursor))
               {
               destination = (intptrj_t)cg()->fe()->methodTrampolineLookup(cg()->comp(), symRef, (void *)cursor);
               TR_ASSERT_FATAL(TR::Compiler->target.cpu.isTargetWithinUnconditionalBranchImmediateRange(destination, (intptrj_t)cursor),
                               "Call target address is out of range");
               }

            intptrj_t distance = destination - (intptrj_t)cursor;
            insertImmediateField(toARM64Cursor(cursor), distance);
            }
         }
      }
   else
      {
      TR_ASSERT(false, "Unsupported opcode in ImmSymInstruction.");
      }

   cursor += ARM64_INSTRUCTION_LENGTH;
   setBinaryLength(ARM64_INSTRUCTION_LENGTH);
   setBinaryEncoding(instructionStart);
   return cursor;
   }

uint8_t *TR::ARM64LabelInstruction::generateBinaryEncoding()
   {
   uint8_t *instructionStart = cg()->getBinaryBufferCursor();
   uint8_t *cursor = instructionStart;
   TR::LabelSymbol *label = getLabelSymbol();

   if (getOpCodeValue() == OMR::InstOpCode::label)
      {
      label->setCodeLocation(instructionStart);
      }
   else
      {
      TR_ASSERT(getOpCodeValue() == OMR::InstOpCode::b, "Unsupported opcode in LabelInstruction.");

      intptr_t destination = (intptr_t)label->getCodeLocation();
      cursor = getOpCode().copyBinaryToBuffer(instructionStart);
      if (destination != 0)
         {
         if (!cg()->directCallRequiresTrampoline(destination, (intptrj_t)cursor))
            {
            intptr_t distance = destination - (intptr_t)cursor;
            insertImmediateField(toARM64Cursor(cursor), distance);
            }
         else
            {
            TR_ASSERT(false, "Branch destination is too far away. Not implemented yet.");
            }
         }
      else
         {
         cg()->addRelocation(new (cg()->trHeapMemory()) TR::LabelRelative32BitRelocation(cursor, label));
         }
      cursor += ARM64_INSTRUCTION_LENGTH;
      }

   setBinaryLength(cursor - instructionStart);
   cg()->addAccumulatedInstructionLengthError(getEstimatedBinaryLength() - getBinaryLength());
   setBinaryEncoding(instructionStart);
   return cursor;
   }

int32_t TR::ARM64LabelInstruction::estimateBinaryLength(int32_t currentEstimate)
   {
   if (getOpCodeValue() == OMR::InstOpCode::label)
      {
      setEstimatedBinaryLength(0);
      getLabelSymbol()->setEstimatedCodeLocation(currentEstimate);
      }
   else
      {
      setEstimatedBinaryLength(ARM64_INSTRUCTION_LENGTH);
      }

   return currentEstimate + getEstimatedBinaryLength();
   }

uint8_t *TR::ARM64ConditionalBranchInstruction::generateBinaryEncoding()
   {
   uint8_t *instructionStart = cg()->getBinaryBufferCursor();
   uint8_t *cursor = instructionStart;
   cursor = getOpCode().copyBinaryToBuffer(instructionStart);
   insertConditionCodeField(toARM64Cursor(cursor));

   TR::LabelSymbol *label = getLabelSymbol();
   uintptr_t destination = (uintptr_t)label->getCodeLocation();
   if (destination != 0)
      {
      intptr_t distance = destination - (uintptr_t)cursor;
      TR_ASSERT(-0x100000 <= distance && distance < 0x100000, "Branch destination is too far away.");
      insertImmediateField(toARM64Cursor(cursor), distance);
      }
   else
      {
      cg()->addRelocation(new (cg()->trHeapMemory()) TR::LabelRelative24BitRelocation(cursor, label));
      }

   cursor += ARM64_INSTRUCTION_LENGTH;
   setBinaryLength(ARM64_INSTRUCTION_LENGTH);
   setBinaryEncoding(instructionStart);
   return cursor;
   }

int32_t TR::ARM64ConditionalBranchInstruction::estimateBinaryLength(int32_t currentEstimate)
   {
   setEstimatedBinaryLength(ARM64_INSTRUCTION_LENGTH);
   return currentEstimate + getEstimatedBinaryLength();
   }

uint8_t *TR::ARM64CompareBranchInstruction::generateBinaryEncoding()
   {
   uint8_t *instructionStart = cg()->getBinaryBufferCursor();
   uint8_t *cursor = instructionStart;
   cursor = getOpCode().copyBinaryToBuffer(instructionStart);
   insertSource1Register(toARM64Cursor(cursor));

   TR::LabelSymbol *label = getLabelSymbol();
   uintptr_t destination = (uintptr_t)label->getCodeLocation();
   if (destination != 0)
      {
      intptr_t distance = destination - (uintptr_t)cursor;
      TR_ASSERT(-0x100000 <= distance && distance < 0x100000, "Branch destination is too far away.");
      insertImmediateField(toARM64Cursor(cursor), distance);
      }
   else
      {
      cg()->addRelocation(new (cg()->trHeapMemory()) TR::LabelRelative24BitRelocation(cursor, label));
      }

   cursor += ARM64_INSTRUCTION_LENGTH;
   setBinaryLength(ARM64_INSTRUCTION_LENGTH);
   setBinaryEncoding(instructionStart);
   return cursor;
   }

uint8_t *TR::ARM64RegBranchInstruction::generateBinaryEncoding()
   {
   uint8_t *instructionStart = cg()->getBinaryBufferCursor();
   uint8_t *cursor = instructionStart;
   cursor = getOpCode().copyBinaryToBuffer(instructionStart);
   insertTargetRegister(toARM64Cursor(cursor));
   cursor += ARM64_INSTRUCTION_LENGTH;
   setBinaryLength(ARM64_INSTRUCTION_LENGTH);
   setBinaryEncoding(instructionStart);
   return cursor;
   }

uint8_t *TR::ARM64AdminInstruction::generateBinaryEncoding()
   {
   uint8_t *instructionStart = cg()->getBinaryBufferCursor();
   TR::InstOpCode::Mnemonic op = getOpCodeValue();
   int32_t i;

   if (op == OMR::InstOpCode::fence)
      {
      TR::Node *fenceNode = getFenceNode();
      uint32_t rtype = fenceNode->getRelocationType();
      if (rtype == TR_AbsoluteAddress)
         {
         for (i = 0; i < fenceNode->getNumRelocations(); i++)
            {
            *(uint8_t **)(fenceNode->getRelocationDestination(i)) = instructionStart;
            }
         }
      else if (rtype == TR_EntryRelative16Bit)
         {
         for (i = 0; i < fenceNode->getNumRelocations(); i++)
            {
            *(uint16_t *)(fenceNode->getRelocationDestination(i)) = (uint16_t)cg()->getCodeLength();
            }
         }
      else // TR_EntryRelative32Bit
         {
         for (i = 0; i < fenceNode->getNumRelocations(); i++)
            {
            *(uint32_t *)(fenceNode->getRelocationDestination(i)) = cg()->getCodeLength();
            }
         }
      }

   setBinaryLength(0);
   setBinaryEncoding(instructionStart);

   return instructionStart;
   }

int32_t TR::ARM64AdminInstruction::estimateBinaryLength(int32_t currentEstimate)
   {
   setEstimatedBinaryLength(0);
   return currentEstimate;
   }

uint8_t *TR::ARM64Trg1Instruction::generateBinaryEncoding()
   {
   uint8_t *instructionStart = cg()->getBinaryBufferCursor();
   uint8_t *cursor = instructionStart;
   cursor = getOpCode().copyBinaryToBuffer(instructionStart);
   insertTargetRegister(toARM64Cursor(cursor));
   cursor += ARM64_INSTRUCTION_LENGTH;
   setBinaryLength(ARM64_INSTRUCTION_LENGTH);
   setBinaryEncoding(instructionStart);
   return cursor;
   }

uint8_t *TR::ARM64Trg1CondInstruction::generateBinaryEncoding()
   {
   uint8_t *instructionStart = cg()->getBinaryBufferCursor();
   uint8_t *cursor = instructionStart;
   cursor = getOpCode().copyBinaryToBuffer(instructionStart);
   insertTargetRegister(toARM64Cursor(cursor));
   insertZeroRegister(toARM64Cursor(cursor));
   insertConditionCodeField(toARM64Cursor(cursor));
   cursor += ARM64_INSTRUCTION_LENGTH;
   setBinaryLength(ARM64_INSTRUCTION_LENGTH);
   setBinaryEncoding(instructionStart);
   return cursor;
   }

uint8_t *TR::ARM64Trg1ImmInstruction::generateBinaryEncoding()
   {
   uint8_t *instructionStart = cg()->getBinaryBufferCursor();
   uint8_t *cursor = instructionStart;
   cursor = getOpCode().copyBinaryToBuffer(instructionStart);
   insertTargetRegister(toARM64Cursor(cursor));
   insertImmediateField(toARM64Cursor(cursor));
   cursor += ARM64_INSTRUCTION_LENGTH;
   setBinaryLength(ARM64_INSTRUCTION_LENGTH);
   setBinaryEncoding(instructionStart);
   return cursor;
   }

uint8_t *TR::ARM64Trg1Src1Instruction::generateBinaryEncoding()
   {
   uint8_t *instructionStart = cg()->getBinaryBufferCursor();
   uint8_t *cursor = instructionStart;
   cursor = getOpCode().copyBinaryToBuffer(instructionStart);
   insertTargetRegister(toARM64Cursor(cursor));
   insertSource1Register(toARM64Cursor(cursor));
   cursor += ARM64_INSTRUCTION_LENGTH;
   setBinaryLength(ARM64_INSTRUCTION_LENGTH);
   setBinaryEncoding(instructionStart);
   return cursor;
   }

uint8_t *TR::ARM64Trg1Src1ImmInstruction::generateBinaryEncoding()
   {
   uint8_t *instructionStart = cg()->getBinaryBufferCursor();
   uint8_t *cursor = instructionStart;
   cursor = getOpCode().copyBinaryToBuffer(instructionStart);
   insertTargetRegister(toARM64Cursor(cursor));
   insertSource1Register(toARM64Cursor(cursor));
   insertImmediateField(toARM64Cursor(cursor));
   insertNbit(toARM64Cursor(cursor));
   cursor += ARM64_INSTRUCTION_LENGTH;
   setBinaryLength(ARM64_INSTRUCTION_LENGTH);
   setBinaryEncoding(instructionStart);
   return cursor;
   }

uint8_t *TR::ARM64Trg1Src2Instruction::generateBinaryEncoding()
   {
   uint8_t *instructionStart = cg()->getBinaryBufferCursor();
   uint8_t *cursor = instructionStart;
   cursor = getOpCode().copyBinaryToBuffer(instructionStart);
   insertTargetRegister(toARM64Cursor(cursor));
   insertSource1Register(toARM64Cursor(cursor));
   insertSource2Register(toARM64Cursor(cursor));
   cursor += ARM64_INSTRUCTION_LENGTH;
   setBinaryLength(ARM64_INSTRUCTION_LENGTH);
   setBinaryEncoding(instructionStart);
   return cursor;
   }

uint8_t *TR::ARM64CondTrg1Src2Instruction::generateBinaryEncoding()
   {
   uint8_t *instructionStart = cg()->getBinaryBufferCursor();
   uint8_t *cursor = instructionStart;
   cursor = getOpCode().copyBinaryToBuffer(instructionStart);
   insertTargetRegister(toARM64Cursor(cursor));
   insertSource1Register(toARM64Cursor(cursor));
   insertSource2Register(toARM64Cursor(cursor));
   insertConditionCodeField(toARM64Cursor(cursor));
   cursor += ARM64_INSTRUCTION_LENGTH;
   setBinaryLength(ARM64_INSTRUCTION_LENGTH);
   setBinaryEncoding(instructionStart);
   return cursor;
   }

uint8_t *TR::ARM64Trg1Src2ShiftedInstruction::generateBinaryEncoding()
   {
   uint8_t *instructionStart = cg()->getBinaryBufferCursor();
   uint8_t *cursor = instructionStart;
   cursor = getOpCode().copyBinaryToBuffer(instructionStart);
   insertTargetRegister(toARM64Cursor(cursor));
   insertSource1Register(toARM64Cursor(cursor));
   insertSource2Register(toARM64Cursor(cursor));
   insertShift(toARM64Cursor(cursor));
   cursor += ARM64_INSTRUCTION_LENGTH;
   setBinaryLength(ARM64_INSTRUCTION_LENGTH);
   setBinaryEncoding(instructionStart);
   return cursor;
   }

uint8_t *TR::ARM64Trg1Src2ExtendedInstruction::generateBinaryEncoding()
   {
   uint8_t *instructionStart = cg()->getBinaryBufferCursor();
   uint8_t *cursor = instructionStart;
   cursor = getOpCode().copyBinaryToBuffer(instructionStart);
   insertTargetRegister(toARM64Cursor(cursor));
   insertSource1Register(toARM64Cursor(cursor));
   insertSource2Register(toARM64Cursor(cursor));
   insertExtend(toARM64Cursor(cursor));
   cursor += ARM64_INSTRUCTION_LENGTH;
   setBinaryLength(ARM64_INSTRUCTION_LENGTH);
   setBinaryEncoding(instructionStart);
   return cursor;
   }

uint8_t *TR::ARM64Trg1Src3Instruction::generateBinaryEncoding()
   {
   uint8_t *instructionStart = cg()->getBinaryBufferCursor();
   uint8_t *cursor = instructionStart;
   cursor = getOpCode().copyBinaryToBuffer(instructionStart);
   insertTargetRegister(toARM64Cursor(cursor));
   insertSource1Register(toARM64Cursor(cursor));
   insertSource2Register(toARM64Cursor(cursor));
   insertSource3Register(toARM64Cursor(cursor));
   cursor += ARM64_INSTRUCTION_LENGTH;
   setBinaryLength(ARM64_INSTRUCTION_LENGTH);
   setBinaryEncoding(instructionStart);
   return cursor;
   }

uint8_t *TR::ARM64Trg1MemInstruction::generateBinaryEncoding()
   {
   uint8_t *instructionStart = cg()->getBinaryBufferCursor();
   uint8_t *cursor = instructionStart;
   cursor = getOpCode().copyBinaryToBuffer(instructionStart);
   insertTargetRegister(toARM64Cursor(cursor));
   cursor = getMemoryReference()->generateBinaryEncoding(this, cursor, cg());
   setBinaryLength(cursor - instructionStart);
   setBinaryEncoding(instructionStart);
   cg()->addAccumulatedInstructionLengthError(getEstimatedBinaryLength() - getBinaryLength());
   return cursor;
   }

int32_t TR::ARM64Trg1MemInstruction::estimateBinaryLength(int32_t currentEstimate)
   {
   setEstimatedBinaryLength(getMemoryReference()->estimateBinaryLength(getOpCodeValue()));
   return currentEstimate + getEstimatedBinaryLength();
   }

uint8_t *TR::ARM64MemInstruction::generateBinaryEncoding()
   {
   uint8_t *instructionStart = cg()->getBinaryBufferCursor();
   uint8_t *cursor = instructionStart;
   cursor = getOpCode().copyBinaryToBuffer(instructionStart);
   cursor = getMemoryReference()->generateBinaryEncoding(this, cursor, cg());
   setBinaryLength(cursor - instructionStart);
   setBinaryEncoding(instructionStart);
   cg()->addAccumulatedInstructionLengthError(getEstimatedBinaryLength() - getBinaryLength());
   return cursor;
   }

int32_t TR::ARM64MemInstruction::estimateBinaryLength(int32_t currentEstimate)
   {
   setEstimatedBinaryLength(getMemoryReference()->estimateBinaryLength(getOpCodeValue()));
   return(currentEstimate + getEstimatedBinaryLength());
   }

uint8_t *TR::ARM64MemSrc1Instruction::generateBinaryEncoding()
   {
   uint8_t *instructionStart = cg()->getBinaryBufferCursor();
   uint8_t *cursor = instructionStart;
   cursor = getOpCode().copyBinaryToBuffer(instructionStart);
   insertSource1Register(toARM64Cursor(cursor));
   cursor = getMemoryReference()->generateBinaryEncoding(this, cursor, cg());
   setBinaryLength(cursor - instructionStart);
   setBinaryEncoding(instructionStart);
   cg()->addAccumulatedInstructionLengthError(getEstimatedBinaryLength() - getBinaryLength());
   return cursor;
   }

int32_t TR::ARM64MemSrc1Instruction::estimateBinaryLength(int32_t currentEstimate)
   {
   setEstimatedBinaryLength(getMemoryReference()->estimateBinaryLength(getOpCodeValue()));
   return(currentEstimate + getEstimatedBinaryLength());
   }

uint8_t *TR::ARM64Src1Instruction::generateBinaryEncoding()
   {
   uint8_t *instructionStart = cg()->getBinaryBufferCursor();
   uint8_t *cursor = instructionStart;
   cursor = getOpCode().copyBinaryToBuffer(instructionStart);
   insertSource1Register(toARM64Cursor(cursor));
   cursor += ARM64_INSTRUCTION_LENGTH;
   setBinaryLength(ARM64_INSTRUCTION_LENGTH);
   setBinaryEncoding(instructionStart);
   return cursor;
   }

uint8_t *TR::ARM64Src2Instruction::generateBinaryEncoding()
   {
   uint8_t *instructionStart = cg()->getBinaryBufferCursor();
   uint8_t *cursor = instructionStart;
   cursor = getOpCode().copyBinaryToBuffer(instructionStart);
   insertSource1Register(toARM64Cursor(cursor));
   insertSource2Register(toARM64Cursor(cursor));
   cursor += ARM64_INSTRUCTION_LENGTH;
   setBinaryLength(ARM64_INSTRUCTION_LENGTH);
   setBinaryEncoding(instructionStart);
   return cursor;
   }
