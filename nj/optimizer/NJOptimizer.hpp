/*******************************************************************************
 * Copyright (c) 2014, 2019 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#ifndef NJ_OPTIMIZER_INCL
#define NJ_OPTIMIZER_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef NJ_OPTIMIZER_CONNECTOR
#define NJ_OPTIMIZER_CONNECTOR
namespace NJCompiler { class Optimizer; }
namespace NJCompiler { typedef NJCompiler::Optimizer OptimizerConnector; }
#endif

#define TR_OVERRIDE_OPTIMIZATION_INITIALIZATION 1

#include "optimizer/OMROptimizer.hpp"

#include <stddef.h>
#include <stdint.h>

namespace TR { class Compilation; }
namespace TR { class Optimizer; }
namespace TR { class ResolvedMethodSymbol; }
struct OptimizationStrategy;

namespace NJCompiler
{

class Optimizer : public OMR::OptimizerConnector
   {
   public:

   Optimizer(TR::Compilation *comp, TR::ResolvedMethodSymbol *methodSymbol, bool isIlGen,
         const OptimizationStrategy *strategy = NULL, uint16_t VNType = 0);

   private:
   TR::Optimizer *self();
   };

}

#endif