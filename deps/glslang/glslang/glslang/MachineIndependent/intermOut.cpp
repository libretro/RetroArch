//
// Copyright (C) 2002-2005  3Dlabs Inc. Ltd.
// Copyright (C) 2012-2016 LunarG, Inc.
// Copyright (C) 2017 ARM Limited.
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//
//    Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//
//    Redistributions in binary form must reproduce the above
//    copyright notice, this list of conditions and the following
//    disclaimer in the documentation and/or other materials provided
//    with the distribution.
//
//    Neither the name of 3Dlabs Inc. Ltd. nor the names of its
//    contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
// FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
// COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
// INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
// BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
// ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//

#include "localintermediate.h"
#include "../Include/InfoSink.h"

#ifdef _MSC_VER
#include <cfloat>
#else
#include <cmath>
#endif

namespace {

static bool IsInfinity(double x) {
#ifdef _MSC_VER
    switch (_fpclass(x)) {
    case _FPCLASS_NINF:
    case _FPCLASS_PINF:
        return true;
    default:
        return false;
    }
#else
    return std::isinf(x);
#endif
}

static bool IsNan(double x) {
#ifdef _MSC_VER
    switch (_fpclass(x)) {
    case _FPCLASS_SNAN:
    case _FPCLASS_QNAN:
        return true;
    default:
	break;
    }
  return false;
#else
  return std::isnan(x);
#endif
}

}

namespace glslang {

//
// Two purposes:
// 1.  Show an example of how to iterate tree.  Functions can
//     also directly call Traverse() on children themselves to
//     have finer grained control over the process than shown here.
//     See the last function for how to get started.
// 2.  Print out a text based description of the tree.
//

//
// Use this class to carry along data from node to node in
// the traversal
//
class TOutputTraverser : public TIntermTraverser {
public:
    TOutputTraverser(TInfoSink& i) : infoSink(i), extraOutput(NoExtraOutput) { }

    enum EExtraOutput {
        NoExtraOutput,
        BinaryDoubleOutput
    };
    void setDoubleOutput(EExtraOutput extra) { extraOutput = extra; }

    virtual bool visitBinary(TVisit, TIntermBinary* node);
    virtual bool visitUnary(TVisit, TIntermUnary* node);
    virtual bool visitAggregate(TVisit, TIntermAggregate* node);
    virtual bool visitSelection(TVisit, TIntermSelection* node);
    virtual void visitConstantUnion(TIntermConstantUnion* node);
    virtual void visitSymbol(TIntermSymbol* node);
    virtual bool visitLoop(TVisit, TIntermLoop* node);
    virtual bool visitBranch(TVisit, TIntermBranch* node);
    virtual bool visitSwitch(TVisit, TIntermSwitch* node);

    TInfoSink& infoSink;
protected:
    TOutputTraverser(TOutputTraverser&);
    TOutputTraverser& operator=(TOutputTraverser&);

    EExtraOutput extraOutput;
};

//
// Helper functions for printing, not part of traversing.
//

static void OutputTreeText(TInfoSink& infoSink, const TIntermNode* node, const int depth)
{
    int i;

    infoSink.debug << node->getLoc().string;
    infoSink.debug.append(":");
    if (node->getLoc().line)
        infoSink.debug << node->getLoc().line;
    else
        infoSink.debug.append("? ");

    for (i = 0; i < depth; ++i)
        infoSink.debug.append("  ");
}

//
// The rest of the file are the traversal functions.  The last one
// is the one that starts the traversal.
//
// Return true from interior nodes to have the external traversal
// continue on to children.  If you process children yourself,
// return false.
//

bool TOutputTraverser::visitBinary(TVisit /* visit */, TIntermBinary* node)
{
    TInfoSink& out = infoSink;

    OutputTreeText(out, node, depth);

    switch (node->getOp()) {
    case EOpAssign:                   out.debug.append("move second child to first child");           break;
    case EOpAddAssign:                out.debug.append("add second child into first child");          break;
    case EOpSubAssign:                out.debug.append("subtract second child into first child");     break;
    case EOpMulAssign:                out.debug.append("multiply second child into first child");     break;
    case EOpVectorTimesMatrixAssign:  out.debug.append("matrix mult second child into first child");  break;
    case EOpVectorTimesScalarAssign:  out.debug.append("vector scale second child into first child"); break;
    case EOpMatrixTimesScalarAssign:  out.debug.append("matrix scale second child into first child"); break;
    case EOpMatrixTimesMatrixAssign:  out.debug.append("matrix mult second child into first child");  break;
    case EOpDivAssign:                out.debug.append("divide second child into first child");       break;
    case EOpModAssign:                out.debug.append("mod second child into first child");          break;
    case EOpAndAssign:                out.debug.append("and second child into first child");          break;
    case EOpInclusiveOrAssign:        out.debug.append("or second child into first child");           break;
    case EOpExclusiveOrAssign:        out.debug.append("exclusive or second child into first child"); break;
    case EOpLeftShiftAssign:          out.debug.append("left shift second child into first child");   break;
    case EOpRightShiftAssign:         out.debug.append("right shift second child into first child");  break;

    case EOpIndexDirect:   out.debug.append("direct index");   break;
    case EOpIndexIndirect: out.debug.append("indirect index"); break;
    case EOpIndexDirectStruct:
        out.debug << (*node->getLeft()->getType().getStruct())[node->getRight()->getAsConstantUnion()->getConstArray()[0].getIConst()].type->getFieldName();
        out.debug.append(": direct index for structure");      break;
    case EOpVectorSwizzle: out.debug.append("vector swizzle"); break;
    case EOpMatrixSwizzle: out.debug.append("matrix swizzle"); break;

    case EOpAdd:    out.debug.append("add");                     break;
    case EOpSub:    out.debug.append("subtract");                break;
    case EOpMul:    out.debug.append("component-wise multiply"); break;
    case EOpDiv:    out.debug.append("divide");                  break;
    case EOpMod:    out.debug.append("mod");                     break;
    case EOpRightShift:  out.debug.append("right-shift");  break;
    case EOpLeftShift:   out.debug.append("left-shift");   break;
    case EOpAnd:         out.debug.append("bitwise and");  break;
    case EOpInclusiveOr: out.debug.append("inclusive-or"); break;
    case EOpExclusiveOr: out.debug.append("exclusive-or"); break;
    case EOpEqual:            out.debug.append("Compare Equal");                 break;
    case EOpNotEqual:         out.debug.append("Compare Not Equal");             break;
    case EOpLessThan:         out.debug.append("Compare Less Than");             break;
    case EOpGreaterThan:      out.debug.append("Compare Greater Than");          break;
    case EOpLessThanEqual:    out.debug.append("Compare Less Than or Equal");    break;
    case EOpGreaterThanEqual: out.debug.append("Compare Greater Than or Equal"); break;
    case EOpVectorEqual:      out.debug.append("Equal");                         break;
    case EOpVectorNotEqual:   out.debug.append("NotEqual");                      break;

    case EOpVectorTimesScalar: out.debug.append("vector-scale");          break;
    case EOpVectorTimesMatrix: out.debug.append("vector-times-matrix");   break;
    case EOpMatrixTimesVector: out.debug.append("matrix-times-vector");   break;
    case EOpMatrixTimesScalar: out.debug.append("matrix-scale");          break;
    case EOpMatrixTimesMatrix: out.debug.append("matrix-multiply");       break;

    case EOpLogicalOr:  out.debug.append("logical-or");   break;
    case EOpLogicalXor: out.debug.append("logical-xor"); break;
    case EOpLogicalAnd: out.debug.append("logical-and"); break;

    default: out.debug.append("<unknown op>");
    }

    out.debug.append(" (");
    out.debug << node->getCompleteString();
    out.debug.append(")");

    out.debug.append("\n");

    return true;
}

bool TOutputTraverser::visitUnary(TVisit /* visit */, TIntermUnary* node)
{
    TInfoSink& out = infoSink;

    OutputTreeText(out, node, depth);

    switch (node->getOp()) {
    case EOpNegative:       out.debug.append("Negate value");         break;
    case EOpVectorLogicalNot:
    case EOpLogicalNot:     out.debug.append("Negate conditional");   break;
    case EOpBitwiseNot:     out.debug.append("Bitwise not");          break;

    case EOpPostIncrement:  out.debug.append("Post-Increment");       break;
    case EOpPostDecrement:  out.debug.append("Post-Decrement");       break;
    case EOpPreIncrement:   out.debug.append("Pre-Increment");        break;
    case EOpPreDecrement:   out.debug.append("Pre-Decrement");        break;

    // * -> bool
    case EOpConvInt8ToBool:    out.debug.append("Convert int8_t to bool");  break;
    case EOpConvUint8ToBool:   out.debug.append("Convert uint8_t to bool"); break;
    case EOpConvInt16ToBool:   out.debug.append("Convert int16_t to bool"); break;
    case EOpConvUint16ToBool:  out.debug.append("Convert uint16_t to bool");break;
    case EOpConvIntToBool:     out.debug.append("Convert int to bool");     break;
    case EOpConvUintToBool:    out.debug.append("Convert uint to bool");    break;
    case EOpConvInt64ToBool:   out.debug.append("Convert int64 to bool");   break;
    case EOpConvUint64ToBool:  out.debug.append("Convert uint64 to bool");  break;
    case EOpConvFloat16ToBool: out.debug.append("Convert float16_t to bool");   break;
    case EOpConvFloatToBool:   out.debug.append("Convert float to bool");   break;
    case EOpConvDoubleToBool:  out.debug.append("Convert double to bool");  break;

    // bool -> *
    case EOpConvBoolToInt8:    out.debug.append("Convert bool to int8_t");  break;
    case EOpConvBoolToUint8:   out.debug.append("Convert bool to uint8_t"); break;
    case EOpConvBoolToInt16:   out.debug.append("Convert bool to in16t_t"); break;
    case EOpConvBoolToUint16:  out.debug.append("Convert bool to uint16_t");break;
    case EOpConvBoolToInt:     out.debug.append("Convert bool to int")  ;   break;
    case EOpConvBoolToUint:    out.debug.append("Convert bool to uint");    break;
    case EOpConvBoolToInt64:   out.debug.append("Convert bool to int64"); break;
    case EOpConvBoolToUint64:  out.debug.append("Convert bool to uint64");break;
    case EOpConvBoolToFloat16: out.debug.append("Convert bool to float16_t");   break;
    case EOpConvBoolToFloat:   out.debug.append("Convert bool to float");   break;
    case EOpConvBoolToDouble:  out.debug.append("Convert bool to double");   break;

    // int8_t -> (u)int*
    case EOpConvInt8ToInt16:   out.debug.append("Convert int8_t to int16_t");break;
    case EOpConvInt8ToInt:     out.debug.append("Convert int8_t to int");    break;
    case EOpConvInt8ToInt64:   out.debug.append("Convert int8_t to int64");   break;
    case EOpConvInt8ToUint8:   out.debug.append("Convert int8_t to uint8_t");break;
    case EOpConvInt8ToUint16:  out.debug.append("Convert int8_t to uint16_t");break;
    case EOpConvInt8ToUint:    out.debug.append("Convert int8_t to uint");    break;
    case EOpConvInt8ToUint64:  out.debug.append("Convert int8_t to uint64");   break;

    // uint8_t -> (u)int*
    case EOpConvUint8ToInt8:    out.debug.append("Convert uint8_t to int8_t");break;
    case EOpConvUint8ToInt16:   out.debug.append("Convert uint8_t to int16_t");break;
    case EOpConvUint8ToInt:     out.debug.append("Convert uint8_t to int");    break;
    case EOpConvUint8ToInt64:   out.debug.append("Convert uint8_t to int64");   break;
    case EOpConvUint8ToUint16:  out.debug.append("Convert uint8_t to uint16_t");break;
    case EOpConvUint8ToUint:    out.debug.append("Convert uint8_t to uint");    break;
    case EOpConvUint8ToUint64:  out.debug.append("Convert uint8_t to uint64");   break;

    // int8_t -> float*
    case EOpConvInt8ToFloat16:  out.debug.append("Convert int8_t to float16_t");break;
    case EOpConvInt8ToFloat:    out.debug.append("Convert int8_t to float");    break;
    case EOpConvInt8ToDouble:   out.debug.append("Convert int8_t to double");   break;

    // uint8_t -> float*
    case EOpConvUint8ToFloat16: out.debug.append("Convert uint8_t to float16_t");break;
    case EOpConvUint8ToFloat:   out.debug.append("Convert uint8_t to float");    break;
    case EOpConvUint8ToDouble:  out.debug.append("Convert uint8_t to double");   break;

    // int16_t -> (u)int*
    case EOpConvInt16ToInt8:    out.debug.append("Convert int16_t to int8_t");break;
    case EOpConvInt16ToInt:     out.debug.append("Convert int16_t to int");    break;
    case EOpConvInt16ToInt64:   out.debug.append("Convert int16_t to int64");   break;
    case EOpConvInt16ToUint8:   out.debug.append("Convert int16_t to uint8_t");break;
    case EOpConvInt16ToUint16:  out.debug.append("Convert int16_t to uint16_t");break;
    case EOpConvInt16ToUint:    out.debug.append("Convert int16_t to uint");    break;
    case EOpConvInt16ToUint64:  out.debug.append("Convert int16_t to uint64");   break;

    // int16_t -> float*
    case EOpConvInt16ToFloat16:  out.debug.append("Convert int16_t to float16_t");break;
    case EOpConvInt16ToFloat:    out.debug.append("Convert int16_t to float");    break;
    case EOpConvInt16ToDouble:   out.debug.append("Convert int16_t to double");   break;

    // uint16_t -> (u)int*
    case EOpConvUint16ToInt8:    out.debug.append("Convert uint16_t to int8_t");break;
    case EOpConvUint16ToInt16:   out.debug.append("Convert uint16_t to int16_t");break;
    case EOpConvUint16ToInt:     out.debug.append("Convert uint16_t to int");    break;
    case EOpConvUint16ToInt64:   out.debug.append("Convert uint16_t to int64");   break;
    case EOpConvUint16ToUint8:   out.debug.append("Convert uint16_t to uint8_t");break;
    case EOpConvUint16ToUint:    out.debug.append("Convert uint16_t to uint");    break;
    case EOpConvUint16ToUint64:  out.debug.append("Convert uint16_t to uint64");   break;

    // uint16_t -> float*
    case EOpConvUint16ToFloat16: out.debug.append("Convert uint16_t to float16_t");break;
    case EOpConvUint16ToFloat:   out.debug.append("Convert uint16_t to float");    break;
    case EOpConvUint16ToDouble:  out.debug.append("Convert uint16_t to double");   break;

    // int32_t -> (u)int*
    case EOpConvIntToInt8:    out.debug.append("Convert int to int8_t");break;
    case EOpConvIntToInt16:   out.debug.append("Convert int to int16_t");break;
    case EOpConvIntToInt64:   out.debug.append("Convert int to int64");   break;
    case EOpConvIntToUint8:   out.debug.append("Convert int to uint8_t");break;
    case EOpConvIntToUint16:  out.debug.append("Convert int to uint16_t");break;
    case EOpConvIntToUint:    out.debug.append("Convert int to uint");    break;
    case EOpConvIntToUint64:  out.debug.append("Convert int to uint64");   break;

    // int32_t -> float*
    case EOpConvIntToFloat16:  out.debug.append("Convert int to float16_t");break;
    case EOpConvIntToFloat:    out.debug.append("Convert int to float");    break;
    case EOpConvIntToDouble:   out.debug.append("Convert int to double");   break;

    // uint32_t -> (u)int*
    case EOpConvUintToInt8:    out.debug.append("Convert uint to int8_t");break;
    case EOpConvUintToInt16:   out.debug.append("Convert uint to int16_t");break;
    case EOpConvUintToInt:     out.debug.append("Convert uint to int");break;
    case EOpConvUintToInt64:   out.debug.append("Convert uint to int64");   break;
    case EOpConvUintToUint8:   out.debug.append("Convert uint to uint8_t");break;
    case EOpConvUintToUint16:  out.debug.append("Convert uint to uint16_t");break;
    case EOpConvUintToUint64:  out.debug.append("Convert uint to uint64");   break;

    // uint32_t -> float*
    case EOpConvUintToFloat16: out.debug.append("Convert uint to float16_t");break;
    case EOpConvUintToFloat:   out.debug.append("Convert uint to float");    break;
    case EOpConvUintToDouble:  out.debug.append("Convert uint to double");   break;

    // int64 -> (u)int*
    case EOpConvInt64ToInt8:    out.debug.append("Convert int64 to int8_t");  break;
    case EOpConvInt64ToInt16:   out.debug.append("Convert int64 to int16_t"); break;
    case EOpConvInt64ToInt:     out.debug.append("Convert int64 to int");   break;
    case EOpConvInt64ToUint8:   out.debug.append("Convert int64 to uint8_t");break;
    case EOpConvInt64ToUint16:  out.debug.append("Convert int64 to uint16_t");break;
    case EOpConvInt64ToUint:    out.debug.append("Convert int64 to uint");    break;
    case EOpConvInt64ToUint64:  out.debug.append("Convert int64 to uint64");   break;

     // int64 -> float*
    case EOpConvInt64ToFloat16:  out.debug.append("Convert int64 to float16_t");break;
    case EOpConvInt64ToFloat:    out.debug.append("Convert int64 to float");    break;
    case EOpConvInt64ToDouble:   out.debug.append("Convert int64 to double");   break;

    // uint64 -> (u)int*
    case EOpConvUint64ToInt8:    out.debug.append("Convert uint64 to int8_t");break;
    case EOpConvUint64ToInt16:   out.debug.append("Convert uint64 to int16_t");break;
    case EOpConvUint64ToInt:     out.debug.append("Convert uint64 to int");    break;
    case EOpConvUint64ToInt64:   out.debug.append("Convert uint64 to int64");   break;
    case EOpConvUint64ToUint8:   out.debug.append("Convert uint64 to uint8_t");break;
    case EOpConvUint64ToUint16:  out.debug.append("Convert uint64 to uint16");    break;
    case EOpConvUint64ToUint:    out.debug.append("Convert uint64 to uint");   break;

    // uint64 -> float*
    case EOpConvUint64ToFloat16: out.debug.append("Convert uint64 to float16_t");break;
    case EOpConvUint64ToFloat:   out.debug.append("Convert uint64 to float");    break;
    case EOpConvUint64ToDouble:  out.debug.append("Convert uint64 to double");   break;

    // float16_t -> int*
    case EOpConvFloat16ToInt8:  out.debug.append("Convert float16_t to int8_t"); break;
    case EOpConvFloat16ToInt16: out.debug.append("Convert float16_t to int16_t"); break;
    case EOpConvFloat16ToInt:   out.debug.append("Convert float16_t to int"); break;
    case EOpConvFloat16ToInt64: out.debug.append("Convert float16_t to int64"); break;

    // float16_t -> uint*
    case EOpConvFloat16ToUint8:  out.debug.append("Convert float16_t to uint8_t"); break;
    case EOpConvFloat16ToUint16: out.debug.append("Convert float16_t to uint16_t"); break;
    case EOpConvFloat16ToUint:   out.debug.append("Convert float16_t to uint"); break;
    case EOpConvFloat16ToUint64: out.debug.append("Convert float16_t to uint64"); break;

    // float16_t -> float*
    case EOpConvFloat16ToFloat:  out.debug.append("Convert float16_t to float"); break;
    case EOpConvFloat16ToDouble: out.debug.append("Convert float16_t to double"); break;

    // float32 -> float*
    case EOpConvFloatToFloat16: out.debug.append("Convert float to float16_t"); break;
    case EOpConvFloatToDouble:  out.debug.append("Convert float to double"); break;

    // float32_t -> int*
    case EOpConvFloatToInt8:  out.debug.append("Convert float to int8_t"); break;
    case EOpConvFloatToInt16: out.debug.append("Convert float to int16_t"); break;
    case EOpConvFloatToInt:   out.debug.append("Convert float to int"); break;
    case EOpConvFloatToInt64: out.debug.append("Convert float to int64"); break;

    // float32_t -> uint*
    case EOpConvFloatToUint8:  out.debug.append("Convert float to uint8_t"); break;
    case EOpConvFloatToUint16: out.debug.append("Convert float to uint16_t"); break;
    case EOpConvFloatToUint:   out.debug.append("Convert float to uint"); break;
    case EOpConvFloatToUint64: out.debug.append("Convert float to uint64"); break;

    // double -> float*
    case EOpConvDoubleToFloat16: out.debug.append("Convert double to float16_t"); break;
    case EOpConvDoubleToFloat:   out.debug.append("Convert double to float"); break;

    // double -> int*
    case EOpConvDoubleToInt8:  out.debug.append("Convert double to int8_t"); break;
    case EOpConvDoubleToInt16: out.debug.append("Convert double to int16_t"); break;
    case EOpConvDoubleToInt:   out.debug.append("Convert double to int"); break;
    case EOpConvDoubleToInt64: out.debug.append("Convert double to int64"); break;

    // float32_t -> uint*
    case EOpConvDoubleToUint8:  out.debug.append("Convert double to uint8_t"); break;
    case EOpConvDoubleToUint16: out.debug.append("Convert double to uint16_t"); break;
    case EOpConvDoubleToUint:   out.debug.append("Convert double to uint"); break;
    case EOpConvDoubleToUint64: out.debug.append("Convert double to uint64"); break;


    case EOpRadians:        out.debug.append("radians");              break;
    case EOpDegrees:        out.debug.append("degrees");              break;
    case EOpSin:            out.debug.append("sine");                 break;
    case EOpCos:            out.debug.append("cosine");               break;
    case EOpTan:            out.debug.append("tangent");              break;
    case EOpAsin:           out.debug.append("arc sine");             break;
    case EOpAcos:           out.debug.append("arc cosine");           break;
    case EOpAtan:           out.debug.append("arc tangent");          break;
    case EOpSinh:           out.debug.append("hyp. sine");            break;
    case EOpCosh:           out.debug.append("hyp. cosine");          break;
    case EOpTanh:           out.debug.append("hyp. tangent");         break;
    case EOpAsinh:          out.debug.append("arc hyp. sine");        break;
    case EOpAcosh:          out.debug.append("arc hyp. cosine");      break;
    case EOpAtanh:          out.debug.append("arc hyp. tangent");     break;

    case EOpExp:            out.debug.append("exp");                  break;
    case EOpLog:            out.debug.append("log");                  break;
    case EOpExp2:           out.debug.append("exp2");                 break;
    case EOpLog2:           out.debug.append("log2");                 break;
    case EOpSqrt:           out.debug.append("sqrt");                 break;
    case EOpInverseSqrt:    out.debug.append("inverse sqrt");         break;

    case EOpAbs:            out.debug.append("Absolute value");       break;
    case EOpSign:           out.debug.append("Sign");                 break;
    case EOpFloor:          out.debug.append("Floor");                break;
    case EOpTrunc:          out.debug.append("trunc");                break;
    case EOpRound:          out.debug.append("round");                break;
    case EOpRoundEven:      out.debug.append("roundEven");            break;
    case EOpCeil:           out.debug.append("Ceiling");              break;
    case EOpFract:          out.debug.append("Fraction");             break;

    case EOpIsNan:          out.debug.append("isnan");                break;
    case EOpIsInf:          out.debug.append("isinf");                break;

    case EOpFloatBitsToInt: out.debug.append("floatBitsToInt");       break;
    case EOpFloatBitsToUint:out.debug.append("floatBitsToUint");      break;
    case EOpIntBitsToFloat: out.debug.append("intBitsToFloat");       break;
    case EOpUintBitsToFloat:out.debug.append("uintBitsToFloat");      break;
    case EOpDoubleBitsToInt64:  out.debug.append("doubleBitsToInt64");  break;
    case EOpDoubleBitsToUint64: out.debug.append("doubleBitsToUint64"); break;
    case EOpInt64BitsToDouble:  out.debug.append("int64BitsToDouble");  break;
    case EOpUint64BitsToDouble: out.debug.append("uint64BitsToDouble"); break;
    case EOpFloat16BitsToInt16:  out.debug.append("float16BitsToInt16");  break;
    case EOpFloat16BitsToUint16: out.debug.append("float16BitsToUint16"); break;
    case EOpInt16BitsToFloat16:  out.debug.append("int16BitsToFloat16");  break;
    case EOpUint16BitsToFloat16: out.debug.append("uint16BitsToFloat16"); break;

    case EOpPackSnorm2x16:  out.debug.append("packSnorm2x16");        break;
    case EOpUnpackSnorm2x16:out.debug.append("unpackSnorm2x16");      break;
    case EOpPackUnorm2x16:  out.debug.append("packUnorm2x16");        break;
    case EOpUnpackUnorm2x16:out.debug.append("unpackUnorm2x16");      break;
    case EOpPackHalf2x16:   out.debug.append("packHalf2x16");         break;
    case EOpUnpackHalf2x16: out.debug.append("unpackHalf2x16");       break;
    case EOpPack16:           out.debug.append("pack16");                 break;
    case EOpPack32:           out.debug.append("pack32");                 break;
    case EOpPack64:           out.debug.append("pack64");                 break;
    case EOpUnpack32:         out.debug.append("unpack32");               break;
    case EOpUnpack16:         out.debug.append("unpack16");               break;
    case EOpUnpack8:          out.debug.append("unpack8");               break;

    case EOpPackSnorm4x8:     out.debug.append("PackSnorm4x8");       break;
    case EOpUnpackSnorm4x8:   out.debug.append("UnpackSnorm4x8");     break;
    case EOpPackUnorm4x8:     out.debug.append("PackUnorm4x8");       break;
    case EOpUnpackUnorm4x8:   out.debug.append("UnpackUnorm4x8");     break;
    case EOpPackDouble2x32:   out.debug.append("PackDouble2x32");     break;
    case EOpUnpackDouble2x32: out.debug.append("UnpackDouble2x32");   break;

    case EOpPackInt2x32:      out.debug.append("packInt2x32");        break;
    case EOpUnpackInt2x32:    out.debug.append("unpackInt2x32");      break;
    case EOpPackUint2x32:     out.debug.append("packUint2x32");       break;
    case EOpUnpackUint2x32:   out.debug.append("unpackUint2x32");     break;

    case EOpPackInt2x16:      out.debug.append("packInt2x16");        break;
    case EOpUnpackInt2x16:    out.debug.append("unpackInt2x16");      break;
    case EOpPackUint2x16:     out.debug.append("packUint2x16");       break;
    case EOpUnpackUint2x16:   out.debug.append("unpackUint2x16");     break;

    case EOpPackInt4x16:      out.debug.append("packInt4x16");        break;
    case EOpUnpackInt4x16:    out.debug.append("unpackInt4x16");      break;
    case EOpPackUint4x16:     out.debug.append("packUint4x16");       break;
    case EOpUnpackUint4x16:   out.debug.append("unpackUint4x16");     break;
    case EOpPackFloat2x16:    out.debug.append("packFloat2x16");      break;
    case EOpUnpackFloat2x16:  out.debug.append("unpackFloat2x16");    break;

    case EOpLength:         out.debug.append("length");               break;
    case EOpNormalize:      out.debug.append("normalize");            break;
    case EOpDPdx:           out.debug.append("dPdx");                 break;
    case EOpDPdy:           out.debug.append("dPdy");                 break;
    case EOpFwidth:         out.debug.append("fwidth");               break;
    case EOpDPdxFine:       out.debug.append("dPdxFine");             break;
    case EOpDPdyFine:       out.debug.append("dPdyFine");             break;
    case EOpFwidthFine:     out.debug.append("fwidthFine");           break;
    case EOpDPdxCoarse:     out.debug.append("dPdxCoarse");           break;
    case EOpDPdyCoarse:     out.debug.append("dPdyCoarse");           break;
    case EOpFwidthCoarse:   out.debug.append("fwidthCoarse");         break;

    case EOpInterpolateAtCentroid: out.debug.append("interpolateAtCentroid");  break;

    case EOpDeterminant:    out.debug.append("determinant");          break;
    case EOpMatrixInverse:  out.debug.append("inverse");              break;
    case EOpTranspose:      out.debug.append("transpose");            break;

    case EOpAny:            out.debug.append("any");                  break;
    case EOpAll:            out.debug.append("all");                  break;

    case EOpArrayLength:    out.debug.append("array length");         break;

    case EOpEmitStreamVertex:   out.debug.append("EmitStreamVertex");   break;
    case EOpEndStreamPrimitive: out.debug.append("EndStreamPrimitive"); break;

    case EOpAtomicCounterIncrement: out.debug.append("AtomicCounterIncrement");break;
    case EOpAtomicCounterDecrement: out.debug.append("AtomicCounterDecrement");break;
    case EOpAtomicCounter:          out.debug.append("AtomicCounter");         break;

    case EOpTextureQuerySize:       out.debug.append("textureSize");           break;
    case EOpTextureQueryLod:        out.debug.append("textureQueryLod");       break;
    case EOpTextureQueryLevels:     out.debug.append("textureQueryLevels");    break;
    case EOpTextureQuerySamples:    out.debug.append("textureSamples");        break;
    case EOpImageQuerySize:         out.debug.append("imageQuerySize");        break;
    case EOpImageQuerySamples:      out.debug.append("imageQuerySamples");     break;
    case EOpImageLoad:              out.debug.append("imageLoad");             break;

    case EOpBitFieldReverse:        out.debug.append("bitFieldReverse");       break;
    case EOpBitCount:               out.debug.append("bitCount");              break;
    case EOpFindLSB:                out.debug.append("findLSB");               break;
    case EOpFindMSB:                out.debug.append("findMSB");               break;

    case EOpNoise:                  out.debug.append("noise");                 break;

    case EOpBallot:                 out.debug.append("ballot");                break;
    case EOpReadFirstInvocation:    out.debug.append("readFirstInvocation");   break;

    case EOpAnyInvocation:          out.debug.append("anyInvocation");         break;
    case EOpAllInvocations:         out.debug.append("allInvocations");        break;
    case EOpAllInvocationsEqual:    out.debug.append("allInvocationsEqual");   break;

    case EOpSubgroupElect:                   out.debug.append("subgroupElect");                   break;
    case EOpSubgroupAll:                     out.debug.append("subgroupAll");                     break;
    case EOpSubgroupAny:                     out.debug.append("subgroupAny");                     break;
    case EOpSubgroupAllEqual:                out.debug.append("subgroupAllEqual");                break;
    case EOpSubgroupBroadcast:               out.debug.append("subgroupBroadcast");               break;
    case EOpSubgroupBroadcastFirst:          out.debug.append("subgroupBroadcastFirst");          break;
    case EOpSubgroupBallot:                  out.debug.append("subgroupBallot");                  break;
    case EOpSubgroupInverseBallot:           out.debug.append("subgroupInverseBallot");           break;
    case EOpSubgroupBallotBitExtract:        out.debug.append("subgroupBallotBitExtract");        break;
    case EOpSubgroupBallotBitCount:          out.debug.append("subgroupBallotBitCount");          break;
    case EOpSubgroupBallotInclusiveBitCount: out.debug.append("subgroupBallotInclusiveBitCount"); break;
    case EOpSubgroupBallotExclusiveBitCount: out.debug.append("subgroupBallotExclusiveBitCount"); break;
    case EOpSubgroupBallotFindLSB:           out.debug.append("subgroupBallotFindLSB");           break;
    case EOpSubgroupBallotFindMSB:           out.debug.append("subgroupBallotFindMSB");           break;
    case EOpSubgroupShuffle:                 out.debug.append("subgroupShuffle");                 break;
    case EOpSubgroupShuffleXor:              out.debug.append("subgroupShuffleXor");              break;
    case EOpSubgroupShuffleUp:               out.debug.append("subgroupShuffleUp");               break;
    case EOpSubgroupShuffleDown:             out.debug.append("subgroupShuffleDown");             break;
    case EOpSubgroupAdd:                     out.debug.append("subgroupAdd");                     break;
    case EOpSubgroupMul:                     out.debug.append("subgroupMul");                     break;
    case EOpSubgroupMin:                     out.debug.append("subgroupMin");                     break;
    case EOpSubgroupMax:                     out.debug.append("subgroupMax");                     break;
    case EOpSubgroupAnd:                     out.debug.append("subgroupAnd");                     break;
    case EOpSubgroupOr:                      out.debug.append("subgroupOr");                      break;
    case EOpSubgroupXor:                     out.debug.append("subgroupXor");                     break;
    case EOpSubgroupInclusiveAdd:            out.debug.append("subgroupInclusiveAdd");            break;
    case EOpSubgroupInclusiveMul:            out.debug.append("subgroupInclusiveMul");            break;
    case EOpSubgroupInclusiveMin:            out.debug.append("subgroupInclusiveMin");            break;
    case EOpSubgroupInclusiveMax:            out.debug.append("subgroupInclusiveMax");            break;
    case EOpSubgroupInclusiveAnd:            out.debug.append("subgroupInclusiveAnd");            break;
    case EOpSubgroupInclusiveOr:             out.debug.append("subgroupInclusiveOr");             break;
    case EOpSubgroupInclusiveXor:            out.debug.append("subgroupInclusiveXor");            break;
    case EOpSubgroupExclusiveAdd:            out.debug.append("subgroupExclusiveAdd");            break;
    case EOpSubgroupExclusiveMul:            out.debug.append("subgroupExclusiveMul");            break;
    case EOpSubgroupExclusiveMin:            out.debug.append("subgroupExclusiveMin");            break;
    case EOpSubgroupExclusiveMax:            out.debug.append("subgroupExclusiveMax");            break;
    case EOpSubgroupExclusiveAnd:            out.debug.append("subgroupExclusiveAnd");            break;
    case EOpSubgroupExclusiveOr:             out.debug.append("subgroupExclusiveOr");             break;
    case EOpSubgroupExclusiveXor:            out.debug.append("subgroupExclusiveXor");            break;
    case EOpSubgroupClusteredAdd:            out.debug.append("subgroupClusteredAdd");            break;
    case EOpSubgroupClusteredMul:            out.debug.append("subgroupClusteredMul");            break;
    case EOpSubgroupClusteredMin:            out.debug.append("subgroupClusteredMin");            break;
    case EOpSubgroupClusteredMax:            out.debug.append("subgroupClusteredMax");            break;
    case EOpSubgroupClusteredAnd:            out.debug.append("subgroupClusteredAnd");            break;
    case EOpSubgroupClusteredOr:             out.debug.append("subgroupClusteredOr");             break;
    case EOpSubgroupClusteredXor:            out.debug.append("subgroupClusteredXor");            break;
    case EOpSubgroupQuadBroadcast:           out.debug.append("subgroupQuadBroadcast");           break;
    case EOpSubgroupQuadSwapHorizontal:      out.debug.append("subgroupQuadSwapHorizontal");      break;
    case EOpSubgroupQuadSwapVertical:        out.debug.append("subgroupQuadSwapVertical");        break;
    case EOpSubgroupQuadSwapDiagonal:        out.debug.append("subgroupQuadSwapDiagonal");        break;

#ifdef NV_EXTENSIONS
    case EOpSubgroupPartition:                          out.debug.append("subgroupPartitionNV");                          break;
    case EOpSubgroupPartitionedAdd:                     out.debug.append("subgroupPartitionedAddNV");                     break;
    case EOpSubgroupPartitionedMul:                     out.debug.append("subgroupPartitionedMulNV");                     break;
    case EOpSubgroupPartitionedMin:                     out.debug.append("subgroupPartitionedMinNV");                     break;
    case EOpSubgroupPartitionedMax:                     out.debug.append("subgroupPartitionedMaxNV");                     break;
    case EOpSubgroupPartitionedAnd:                     out.debug.append("subgroupPartitionedAndNV");                     break;
    case EOpSubgroupPartitionedOr:                      out.debug.append("subgroupPartitionedOrNV");                      break;
    case EOpSubgroupPartitionedXor:                     out.debug.append("subgroupPartitionedXorNV");                     break;
    case EOpSubgroupPartitionedInclusiveAdd:            out.debug.append("subgroupPartitionedInclusiveAddNV");            break;
    case EOpSubgroupPartitionedInclusiveMul:            out.debug.append("subgroupPartitionedInclusiveMulNV");            break;
    case EOpSubgroupPartitionedInclusiveMin:            out.debug.append("subgroupPartitionedInclusiveMinNV");            break;
    case EOpSubgroupPartitionedInclusiveMax:            out.debug.append("subgroupPartitionedInclusiveMaxNV");            break;
    case EOpSubgroupPartitionedInclusiveAnd:            out.debug.append("subgroupPartitionedInclusiveAndNV");            break;
    case EOpSubgroupPartitionedInclusiveOr:             out.debug.append("subgroupPartitionedInclusiveOrNV");             break;
    case EOpSubgroupPartitionedInclusiveXor:            out.debug.append("subgroupPartitionedInclusiveXorNV");            break;
    case EOpSubgroupPartitionedExclusiveAdd:            out.debug.append("subgroupPartitionedExclusiveAddNV");            break;
    case EOpSubgroupPartitionedExclusiveMul:            out.debug.append("subgroupPartitionedExclusiveMulNV");            break;
    case EOpSubgroupPartitionedExclusiveMin:            out.debug.append("subgroupPartitionedExclusiveMinNV");            break;
    case EOpSubgroupPartitionedExclusiveMax:            out.debug.append("subgroupPartitionedExclusiveMaxNV");            break;
    case EOpSubgroupPartitionedExclusiveAnd:            out.debug.append("subgroupPartitionedExclusiveAndNV");            break;
    case EOpSubgroupPartitionedExclusiveOr:             out.debug.append("subgroupPartitionedExclusiveOrNV");             break;
    case EOpSubgroupPartitionedExclusiveXor:            out.debug.append("subgroupPartitionedExclusiveXorNV");            break;
#endif

    case EOpClip:                   out.debug.append("clip");                  break;
    case EOpIsFinite:               out.debug.append("isfinite");              break;
    case EOpLog10:                  out.debug.append("log10");                 break;
    case EOpRcp:                    out.debug.append("rcp");                   break;
    case EOpSaturate:               out.debug.append("saturate");              break;

    case EOpSparseTexelsResident:   out.debug.append("sparseTexelsResident");  break;

#ifdef AMD_EXTENSIONS
    case EOpMinInvocations:             out.debug.append("minInvocations");              break;
    case EOpMaxInvocations:             out.debug.append("maxInvocations");              break;
    case EOpAddInvocations:             out.debug.append("addInvocations");              break;
    case EOpMinInvocationsNonUniform:   out.debug.append("minInvocationsNonUniform");    break;
    case EOpMaxInvocationsNonUniform:   out.debug.append("maxInvocationsNonUniform");    break;
    case EOpAddInvocationsNonUniform:   out.debug.append("addInvocationsNonUniform");    break;

    case EOpMinInvocationsInclusiveScan:            out.debug.append("minInvocationsInclusiveScan");             break;
    case EOpMaxInvocationsInclusiveScan:            out.debug.append("maxInvocationsInclusiveScan");             break;
    case EOpAddInvocationsInclusiveScan:            out.debug.append("addInvocationsInclusiveScan");             break;
    case EOpMinInvocationsInclusiveScanNonUniform:  out.debug.append("minInvocationsInclusiveScanNonUniform");   break;
    case EOpMaxInvocationsInclusiveScanNonUniform:  out.debug.append("maxInvocationsInclusiveScanNonUniform");   break;
    case EOpAddInvocationsInclusiveScanNonUniform:  out.debug.append("addInvocationsInclusiveScanNonUniform");   break;

    case EOpMinInvocationsExclusiveScan:            out.debug.append("minInvocationsExclusiveScan");             break;
    case EOpMaxInvocationsExclusiveScan:            out.debug.append("maxInvocationsExclusiveScan");             break;
    case EOpAddInvocationsExclusiveScan:            out.debug.append("addInvocationsExclusiveScan");             break;
    case EOpMinInvocationsExclusiveScanNonUniform:  out.debug.append("minInvocationsExclusiveScanNonUniform");   break;
    case EOpMaxInvocationsExclusiveScanNonUniform:  out.debug.append("maxInvocationsExclusiveScanNonUniform");   break;
    case EOpAddInvocationsExclusiveScanNonUniform:  out.debug.append("addInvocationsExclusiveScanNonUniform");   break;

    case EOpMbcnt:                  out.debug.append("mbcnt");                       break;

    case EOpFragmentMaskFetch:      out.debug.append("fragmentMaskFetchAMD");        break;
    case EOpFragmentFetch:          out.debug.append("fragmentFetchAMD");            break;

    case EOpCubeFaceIndex:          out.debug.append("cubeFaceIndex");               break;
    case EOpCubeFaceCoord:          out.debug.append("cubeFaceCoord");               break;
#endif

    case EOpSubpassLoad:   out.debug.append("subpassLoad");   break;
    case EOpSubpassLoadMS: out.debug.append("subpassLoadMS"); break;

    default:
			   out.debug.append("ERROR: Bad unary op\n");
			   break;
    }

    out.debug.append(" (");
    out.debug << node->getCompleteString();
    out.debug.append(")");

    out.debug.append("\n");

    return true;
}

bool TOutputTraverser::visitAggregate(TVisit /* visit */, TIntermAggregate* node)
{
    TInfoSink& out = infoSink;

    if (node->getOp() == EOpNull) {
	out.debug.append("ERROR: node is still EOpNull!\n");
        return true;
    }

    OutputTreeText(out, node, depth);

    switch (node->getOp()) {
    case EOpSequence:      out.debug.append("Sequence\n");       return true;
    case EOpLinkerObjects: out.debug.append("Linker Objects\n"); return true;
    case EOpComma:         out.debug.append("Comma");            break;
    case EOpFunction:      out.debug.append("Function Definition: "); out.debug << node->getName(); break;
    case EOpFunctionCall:  out.debug.append("Function Call: "); out.debug << node->getName(); break;
    case EOpParameters:    out.debug.append("Function Parameters: ");                    break;

    case EOpConstructFloat: out.debug.append("Construct float"); break;
    case EOpConstructDouble:out.debug.append("Construct double"); break;

    case EOpConstructVec2:  out.debug.append("Construct vec2");  break;
    case EOpConstructVec3:  out.debug.append("Construct vec3");  break;
    case EOpConstructVec4:  out.debug.append("Construct vec4");  break;
    case EOpConstructDVec2: out.debug.append("Construct dvec2");  break;
    case EOpConstructDVec3: out.debug.append("Construct dvec3");  break;
    case EOpConstructDVec4: out.debug.append("Construct dvec4");  break;
    case EOpConstructBool:  out.debug.append("Construct bool");  break;
    case EOpConstructBVec2: out.debug.append("Construct bvec2"); break;
    case EOpConstructBVec3: out.debug.append("Construct bvec3"); break;
    case EOpConstructBVec4: out.debug.append("Construct bvec4"); break;
    case EOpConstructInt8:  out.debug.append("Construct int8_t");   break;
    case EOpConstructI8Vec2: out.debug.append("Construct i8vec2"); break;
    case EOpConstructI8Vec3: out.debug.append("Construct i8vec3"); break;
    case EOpConstructI8Vec4: out.debug.append("Construct i8vec4"); break;
    case EOpConstructInt:   out.debug.append("Construct int");   break;
    case EOpConstructIVec2: out.debug.append("Construct ivec2"); break;
    case EOpConstructIVec3: out.debug.append("Construct ivec3"); break;
    case EOpConstructIVec4: out.debug.append("Construct ivec4"); break;
    case EOpConstructUint8: out.debug.append("Construct uint8_t");    break;
    case EOpConstructU8Vec2:   out.debug.append("Construct u8vec2");   break;
    case EOpConstructU8Vec3:   out.debug.append("Construct u8vec3");   break;
    case EOpConstructU8Vec4:   out.debug.append("Construct u8vec4");   break;
    case EOpConstructUint:    out.debug.append("Construct uint");    break;
    case EOpConstructUVec2:   out.debug.append("Construct uvec2");   break;
    case EOpConstructUVec3:   out.debug.append("Construct uvec3");   break;
    case EOpConstructUVec4:   out.debug.append("Construct uvec4");   break;
    case EOpConstructInt64:   out.debug.append("Construct int64"); break;
    case EOpConstructI64Vec2: out.debug.append("Construct i64vec2"); break;
    case EOpConstructI64Vec3: out.debug.append("Construct i64vec3"); break;
    case EOpConstructI64Vec4: out.debug.append("Construct i64vec4"); break;
    case EOpConstructUint64:  out.debug.append("Construct uint64"); break;
    case EOpConstructU64Vec2: out.debug.append("Construct u64vec2"); break;
    case EOpConstructU64Vec3: out.debug.append("Construct u64vec3"); break;
    case EOpConstructU64Vec4: out.debug.append("Construct u64vec4"); break;
    case EOpConstructInt16:   out.debug.append("Construct int16_t"); break;
    case EOpConstructI16Vec2: out.debug.append("Construct i16vec2"); break;
    case EOpConstructI16Vec3: out.debug.append("Construct i16vec3"); break;
    case EOpConstructI16Vec4: out.debug.append("Construct i16vec4"); break;
    case EOpConstructUint16:  out.debug.append("Construct uint16_t"); break;
    case EOpConstructU16Vec2: out.debug.append("Construct u16vec2"); break;
    case EOpConstructU16Vec3: out.debug.append("Construct u16vec3"); break;
    case EOpConstructU16Vec4: out.debug.append("Construct u16vec4"); break;
    case EOpConstructMat2x2:  out.debug.append("Construct mat2");    break;
    case EOpConstructMat2x3:  out.debug.append("Construct mat2x3");  break;
    case EOpConstructMat2x4:  out.debug.append("Construct mat2x4");  break;
    case EOpConstructMat3x2:  out.debug.append("Construct mat3x2");  break;
    case EOpConstructMat3x3:  out.debug.append("Construct mat3");    break;
    case EOpConstructMat3x4:  out.debug.append("Construct mat3x4");  break;
    case EOpConstructMat4x2:  out.debug.append("Construct mat4x2");  break;
    case EOpConstructMat4x3:  out.debug.append("Construct mat4x3");  break;
    case EOpConstructMat4x4:  out.debug.append("Construct mat4");    break;
    case EOpConstructDMat2x2: out.debug.append("Construct dmat2");   break;
    case EOpConstructDMat2x3: out.debug.append("Construct dmat2x3"); break;
    case EOpConstructDMat2x4: out.debug.append("Construct dmat2x4"); break;
    case EOpConstructDMat3x2: out.debug.append("Construct dmat3x2"); break;
    case EOpConstructDMat3x3: out.debug.append("Construct dmat3");   break;
    case EOpConstructDMat3x4: out.debug.append("Construct dmat3x4"); break;
    case EOpConstructDMat4x2: out.debug.append("Construct dmat4x2"); break;
    case EOpConstructDMat4x3: out.debug.append("Construct dmat4x3"); break;
    case EOpConstructDMat4x4: out.debug.append("Construct dmat4");   break;
    case EOpConstructIMat2x2: out.debug.append("Construct imat2");   break;
    case EOpConstructIMat2x3: out.debug.append("Construct imat2x3"); break;
    case EOpConstructIMat2x4: out.debug.append("Construct imat2x4"); break;
    case EOpConstructIMat3x2: out.debug.append("Construct imat3x2"); break;
    case EOpConstructIMat3x3: out.debug.append("Construct imat3");   break;
    case EOpConstructIMat3x4: out.debug.append("Construct imat3x4"); break;
    case EOpConstructIMat4x2: out.debug.append("Construct imat4x2"); break;
    case EOpConstructIMat4x3: out.debug.append("Construct imat4x3"); break;
    case EOpConstructIMat4x4: out.debug.append("Construct imat4");   break;
    case EOpConstructUMat2x2: out.debug.append("Construct umat2");   break;
    case EOpConstructUMat2x3: out.debug.append("Construct umat2x3"); break;
    case EOpConstructUMat2x4: out.debug.append("Construct umat2x4"); break;
    case EOpConstructUMat3x2: out.debug.append("Construct umat3x2"); break;
    case EOpConstructUMat3x3: out.debug.append("Construct umat3");   break;
    case EOpConstructUMat3x4: out.debug.append("Construct umat3x4"); break;
    case EOpConstructUMat4x2: out.debug.append("Construct umat4x2"); break;
    case EOpConstructUMat4x3: out.debug.append("Construct umat4x3"); break;
    case EOpConstructUMat4x4: out.debug.append("Construct umat4");   break;
    case EOpConstructBMat2x2: out.debug.append("Construct bmat2");   break;
    case EOpConstructBMat2x3: out.debug.append("Construct bmat2x3"); break;
    case EOpConstructBMat2x4: out.debug.append("Construct bmat2x4"); break;
    case EOpConstructBMat3x2: out.debug.append("Construct bmat3x2"); break;
    case EOpConstructBMat3x3: out.debug.append("Construct bmat3");   break;
    case EOpConstructBMat3x4: out.debug.append("Construct bmat3x4"); break;
    case EOpConstructBMat4x2: out.debug.append("Construct bmat4x2"); break;
    case EOpConstructBMat4x3: out.debug.append("Construct bmat4x3"); break;
    case EOpConstructBMat4x4: out.debug.append("Construct bmat4");   break;
    case EOpConstructFloat16:   out.debug.append("Construct float16_t"); break;
    case EOpConstructF16Vec2:   out.debug.append("Construct f16vec2");   break;
    case EOpConstructF16Vec3:   out.debug.append("Construct f16vec3");   break;
    case EOpConstructF16Vec4:   out.debug.append("Construct f16vec4");   break;
    case EOpConstructF16Mat2x2: out.debug.append("Construct f16mat2");   break;
    case EOpConstructF16Mat2x3: out.debug.append("Construct f16mat2x3"); break;
    case EOpConstructF16Mat2x4: out.debug.append("Construct f16mat2x4"); break;
    case EOpConstructF16Mat3x2: out.debug.append("Construct f16mat3x2"); break;
    case EOpConstructF16Mat3x3: out.debug.append("Construct f16mat3");   break;
    case EOpConstructF16Mat3x4: out.debug.append("Construct f16mat3x4"); break;
    case EOpConstructF16Mat4x2: out.debug.append("Construct f16mat4x2"); break;
    case EOpConstructF16Mat4x3: out.debug.append("Construct f16mat4x3"); break;
    case EOpConstructF16Mat4x4: out.debug.append("Construct f16mat4");   break;
    case EOpConstructStruct:  out.debug.append("Construct structure");  break;
    case EOpConstructTextureSampler: out.debug.append("Construct combined texture-sampler"); break;

    case EOpLessThan:         out.debug.append("Compare Less Than");             break;
    case EOpGreaterThan:      out.debug.append("Compare Greater Than");          break;
    case EOpLessThanEqual:    out.debug.append("Compare Less Than or Equal");    break;
    case EOpGreaterThanEqual: out.debug.append("Compare Greater Than or Equal"); break;
    case EOpVectorEqual:      out.debug.append("Equal");                         break;
    case EOpVectorNotEqual:   out.debug.append("NotEqual");                      break;

    case EOpMod:           out.debug.append("mod");         break;
    case EOpModf:          out.debug.append("modf");        break;
    case EOpPow:           out.debug.append("pow");         break;

    case EOpAtan:          out.debug.append("arc tangent"); break;

    case EOpMin:           out.debug.append("min");         break;
    case EOpMax:           out.debug.append("max");         break;
    case EOpClamp:         out.debug.append("clamp");       break;
    case EOpMix:           out.debug.append("mix");         break;
    case EOpStep:          out.debug.append("step");        break;
    case EOpSmoothStep:    out.debug.append("smoothstep");  break;

    case EOpDistance:      out.debug.append("distance");                break;
    case EOpDot:           out.debug.append("dot-product");             break;
    case EOpCross:         out.debug.append("cross-product");           break;
    case EOpFaceForward:   out.debug.append("face-forward");            break;
    case EOpReflect:       out.debug.append("reflect");                 break;
    case EOpRefract:       out.debug.append("refract");                 break;
    case EOpMul:           out.debug.append("component-wise multiply"); break;
    case EOpOuterProduct:  out.debug.append("outer product");           break;

    case EOpEmitVertex:    out.debug.append("EmitVertex");              break;
    case EOpEndPrimitive:  out.debug.append("EndPrimitive");            break;

    case EOpBarrier:                    out.debug.append("Barrier");                    break;
    case EOpMemoryBarrier:              out.debug.append("MemoryBarrier");              break;
    case EOpMemoryBarrierAtomicCounter: out.debug.append("MemoryBarrierAtomicCounter"); break;
    case EOpMemoryBarrierBuffer:        out.debug.append("MemoryBarrierBuffer");        break;
    case EOpMemoryBarrierImage:         out.debug.append("MemoryBarrierImage");         break;
    case EOpMemoryBarrierShared:        out.debug.append("MemoryBarrierShared");        break;
    case EOpGroupMemoryBarrier:         out.debug.append("GroupMemoryBarrier");         break;

    case EOpReadInvocation:             out.debug.append("readInvocation");        break;

#ifdef AMD_EXTENSIONS
    case EOpSwizzleInvocations:         out.debug.append("swizzleInvocations");       break;
    case EOpSwizzleInvocationsMasked:   out.debug.append("swizzleInvocationsMasked"); break;
    case EOpWriteInvocation:            out.debug.append("writeInvocation");          break;

    case EOpMin3:                       out.debug.append("min3");                  break;
    case EOpMax3:                       out.debug.append("max3");                  break;
    case EOpMid3:                       out.debug.append("mid3");                  break;

    case EOpTime:                       out.debug.append("time");                  break;
#endif

    case EOpAtomicAdd:                  out.debug.append("AtomicAdd");             break;
    case EOpAtomicMin:                  out.debug.append("AtomicMin");             break;
    case EOpAtomicMax:                  out.debug.append("AtomicMax");             break;
    case EOpAtomicAnd:                  out.debug.append("AtomicAnd");             break;
    case EOpAtomicOr:                   out.debug.append("AtomicOr");              break;
    case EOpAtomicXor:                  out.debug.append("AtomicXor");             break;
    case EOpAtomicExchange:             out.debug.append("AtomicExchange");        break;
    case EOpAtomicCompSwap:             out.debug.append("AtomicCompSwap");        break;

    case EOpAtomicCounterAdd:           out.debug.append("AtomicCounterAdd");      break;
    case EOpAtomicCounterSubtract:      out.debug.append("AtomicCounterSubtract"); break;
    case EOpAtomicCounterMin:           out.debug.append("AtomicCounterMin");      break;
    case EOpAtomicCounterMax:           out.debug.append("AtomicCounterMax");      break;
    case EOpAtomicCounterAnd:           out.debug.append("AtomicCounterAnd");      break;
    case EOpAtomicCounterOr:            out.debug.append("AtomicCounterOr");       break;
    case EOpAtomicCounterXor:           out.debug.append("AtomicCounterXor");      break;
    case EOpAtomicCounterExchange:      out.debug.append("AtomicCounterExchange"); break;
    case EOpAtomicCounterCompSwap:      out.debug.append("AtomicCounterCompSwap"); break;

    case EOpImageQuerySize:             out.debug.append("imageQuerySize");        break;
    case EOpImageQuerySamples:          out.debug.append("imageQuerySamples");     break;
    case EOpImageLoad:                  out.debug.append("imageLoad");             break;
    case EOpImageStore:                 out.debug.append("imageStore");            break;
    case EOpImageAtomicAdd:             out.debug.append("imageAtomicAdd");        break;
    case EOpImageAtomicMin:             out.debug.append("imageAtomicMin");        break;
    case EOpImageAtomicMax:             out.debug.append("imageAtomicMax");        break;
    case EOpImageAtomicAnd:             out.debug.append("imageAtomicAnd");        break;
    case EOpImageAtomicOr:              out.debug.append("imageAtomicOr");         break;
    case EOpImageAtomicXor:             out.debug.append("imageAtomicXor");        break;
    case EOpImageAtomicExchange:        out.debug.append("imageAtomicExchange");   break;
    case EOpImageAtomicCompSwap:        out.debug.append("imageAtomicCompSwap");   break;
#ifdef AMD_EXTENSIONS
    case EOpImageLoadLod:               out.debug.append("imageLoadLod");          break;
    case EOpImageStoreLod:              out.debug.append("imageStoreLod");         break;
#endif

    case EOpTextureQuerySize:           out.debug.append("textureSize");           break;
    case EOpTextureQueryLod:            out.debug.append("textureQueryLod");       break;
    case EOpTextureQueryLevels:         out.debug.append("textureQueryLevels");    break;
    case EOpTextureQuerySamples:        out.debug.append("textureSamples");        break;
    case EOpTexture:                    out.debug.append("texture");               break;
    case EOpTextureProj:                out.debug.append("textureProj");           break;
    case EOpTextureLod:                 out.debug.append("textureLod");            break;
    case EOpTextureOffset:              out.debug.append("textureOffset");         break;
    case EOpTextureFetch:               out.debug.append("textureFetch");          break;
    case EOpTextureFetchOffset:         out.debug.append("textureFetchOffset");    break;
    case EOpTextureProjOffset:          out.debug.append("textureProjOffset");     break;
    case EOpTextureLodOffset:           out.debug.append("textureLodOffset");      break;
    case EOpTextureProjLod:             out.debug.append("textureProjLod");        break;
    case EOpTextureProjLodOffset:       out.debug.append("textureProjLodOffset");  break;
    case EOpTextureGrad:                out.debug.append("textureGrad");           break;
    case EOpTextureGradOffset:          out.debug.append("textureGradOffset");     break;
    case EOpTextureProjGrad:            out.debug.append("textureProjGrad");       break;
    case EOpTextureProjGradOffset:      out.debug.append("textureProjGradOffset"); break;
    case EOpTextureGather:              out.debug.append("textureGather");         break;
    case EOpTextureGatherOffset:        out.debug.append("textureGatherOffset");   break;
    case EOpTextureGatherOffsets:       out.debug.append("textureGatherOffsets");  break;
    case EOpTextureClamp:               out.debug.append("textureClamp");          break;
    case EOpTextureOffsetClamp:         out.debug.append("textureOffsetClamp");    break;
    case EOpTextureGradClamp:           out.debug.append("textureGradClamp");      break;
    case EOpTextureGradOffsetClamp:     out.debug.append("textureGradOffsetClamp");  break;
#ifdef AMD_EXTENSIONS
    case EOpTextureGatherLod:           out.debug.append("textureGatherLod");        break;
    case EOpTextureGatherLodOffset:     out.debug.append("textureGatherLodOffset");  break;
    case EOpTextureGatherLodOffsets:    out.debug.append("textureGatherLodOffsets"); break;
#endif

    case EOpSparseTexture:                  out.debug.append("sparseTexture");                   break;
    case EOpSparseTextureOffset:            out.debug.append("sparseTextureOffset");             break;
    case EOpSparseTextureLod:               out.debug.append("sparseTextureLod");                break;
    case EOpSparseTextureLodOffset:         out.debug.append("sparseTextureLodOffset");          break;
    case EOpSparseTextureFetch:             out.debug.append("sparseTexelFetch");                break;
    case EOpSparseTextureFetchOffset:       out.debug.append("sparseTexelFetchOffset");          break;
    case EOpSparseTextureGrad:              out.debug.append("sparseTextureGrad");               break;
    case EOpSparseTextureGradOffset:        out.debug.append("sparseTextureGradOffset");         break;
    case EOpSparseTextureGather:            out.debug.append("sparseTextureGather");             break;
    case EOpSparseTextureGatherOffset:      out.debug.append("sparseTextureGatherOffset");       break;
    case EOpSparseTextureGatherOffsets:     out.debug.append("sparseTextureGatherOffsets");      break;
    case EOpSparseImageLoad:                out.debug.append("sparseImageLoad");                 break;
    case EOpSparseTextureClamp:             out.debug.append("sparseTextureClamp");              break;
    case EOpSparseTextureOffsetClamp:       out.debug.append("sparseTextureOffsetClamp");        break;
    case EOpSparseTextureGradClamp:         out.debug.append("sparseTextureGradClamp");          break;
    case EOpSparseTextureGradOffsetClamp:   out.debug.append("sparseTextureGradOffsetClam");     break;
#ifdef AMD_EXTENSIONS
    case EOpSparseTextureGatherLod:         out.debug.append("sparseTextureGatherLod");          break;
    case EOpSparseTextureGatherLodOffset:   out.debug.append("sparseTextureGatherLodOffset");    break;
    case EOpSparseTextureGatherLodOffsets:  out.debug.append("sparseTextureGatherLodOffsets");   break;
    case EOpSparseImageLoadLod:             out.debug.append("sparseImageLoadLod");              break;
#endif

    case EOpAddCarry:                   out.debug.append("addCarry");              break;
    case EOpSubBorrow:                  out.debug.append("subBorrow");             break;
    case EOpUMulExtended:               out.debug.append("uMulExtended");          break;
    case EOpIMulExtended:               out.debug.append("iMulExtended");          break;
    case EOpBitfieldExtract:            out.debug.append("bitfieldExtract");       break;
    case EOpBitfieldInsert:             out.debug.append("bitfieldInsert");        break;

    case EOpFma:                        out.debug.append("fma");                   break;
    case EOpFrexp:                      out.debug.append("frexp");                 break;
    case EOpLdexp:                      out.debug.append("ldexp");                 break;

    case EOpInterpolateAtSample:   out.debug.append("interpolateAtSample");    break;
    case EOpInterpolateAtOffset:   out.debug.append("interpolateAtOffset");    break;
#ifdef AMD_EXTENSIONS
    case EOpInterpolateAtVertex:   out.debug.append("interpolateAtVertex");    break;
#endif

    case EOpSinCos:                     out.debug.append("sincos");                break;
    case EOpGenMul:                     out.debug.append("mul");                   break;

    case EOpAllMemoryBarrierWithGroupSync:    out.debug.append("AllMemoryBarrierWithGroupSync");    break;
    case EOpDeviceMemoryBarrier:              out.debug.append("DeviceMemoryBarrier");              break;
    case EOpDeviceMemoryBarrierWithGroupSync: out.debug.append("DeviceMemoryBarrierWithGroupSync"); break;
    case EOpWorkgroupMemoryBarrier:           out.debug.append("WorkgroupMemoryBarrier");           break;
    case EOpWorkgroupMemoryBarrierWithGroupSync: out.debug.append("WorkgroupMemoryBarrierWithGroupSync"); break;

    case EOpSubgroupBarrier:                 out.debug.append("subgroupBarrier"); break;
    case EOpSubgroupMemoryBarrier:           out.debug.append("subgroupMemoryBarrier"); break;
    case EOpSubgroupMemoryBarrierBuffer:     out.debug.append("subgroupMemoryBarrierBuffer"); break;
    case EOpSubgroupMemoryBarrierImage:      out.debug.append("subgroupMemoryBarrierImage");   break;
    case EOpSubgroupMemoryBarrierShared:     out.debug.append("subgroupMemoryBarrierShared"); break;
    case EOpSubgroupElect:                   out.debug.append("subgroupElect"); break;
    case EOpSubgroupAll:                     out.debug.append("subgroupAll"); break;
    case EOpSubgroupAny:                     out.debug.append("subgroupAny"); break;
    case EOpSubgroupAllEqual:                out.debug.append("subgroupAllEqual"); break;
    case EOpSubgroupBroadcast:               out.debug.append("subgroupBroadcast"); break;
    case EOpSubgroupBroadcastFirst:          out.debug.append("subgroupBroadcastFirst"); break;
    case EOpSubgroupBallot:                  out.debug.append("subgroupBallot"); break;
    case EOpSubgroupInverseBallot:           out.debug.append("subgroupInverseBallot"); break;
    case EOpSubgroupBallotBitExtract:        out.debug.append("subgroupBallotBitExtract"); break;
    case EOpSubgroupBallotBitCount:          out.debug.append("subgroupBallotBitCount"); break;
    case EOpSubgroupBallotInclusiveBitCount: out.debug.append("subgroupBallotInclusiveBitCount"); break;
    case EOpSubgroupBallotExclusiveBitCount: out.debug.append("subgroupBallotExclusiveBitCount"); break;
    case EOpSubgroupBallotFindLSB:           out.debug.append("subgroupBallotFindLSB"); break;
    case EOpSubgroupBallotFindMSB:           out.debug.append("subgroupBallotFindMSB"); break;
    case EOpSubgroupShuffle:                 out.debug.append("subgroupShuffle"); break;
    case EOpSubgroupShuffleXor:              out.debug.append("subgroupShuffleXor"); break;
    case EOpSubgroupShuffleUp:               out.debug.append("subgroupShuffleUp"); break;
    case EOpSubgroupShuffleDown:             out.debug.append("subgroupShuffleDown"); break;
    case EOpSubgroupAdd:                     out.debug.append("subgroupAdd"); break;
    case EOpSubgroupMul:                     out.debug.append("subgroupMul"); break;
    case EOpSubgroupMin:                     out.debug.append("subgroupMin"); break;
    case EOpSubgroupMax:                     out.debug.append("subgroupMax"); break;
    case EOpSubgroupAnd:                     out.debug.append("subgroupAnd"); break;
    case EOpSubgroupOr:                      out.debug.append("subgroupOr"); break;
    case EOpSubgroupXor:                     out.debug.append("subgroupXor"); break;
    case EOpSubgroupInclusiveAdd:            out.debug.append("subgroupInclusiveAdd"); break;
    case EOpSubgroupInclusiveMul:            out.debug.append("subgroupInclusiveMul"); break;
    case EOpSubgroupInclusiveMin:            out.debug.append("subgroupInclusiveMin"); break;
    case EOpSubgroupInclusiveMax:            out.debug.append("subgroupInclusiveMax"); break;
    case EOpSubgroupInclusiveAnd:            out.debug.append("subgroupInclusiveAnd"); break;
    case EOpSubgroupInclusiveOr:             out.debug.append("subgroupInclusiveOr"); break;
    case EOpSubgroupInclusiveXor:            out.debug.append("subgroupInclusiveXor"); break;
    case EOpSubgroupExclusiveAdd:            out.debug.append("subgroupExclusiveAdd"); break;
    case EOpSubgroupExclusiveMul:            out.debug.append("subgroupExclusiveMul"); break;
    case EOpSubgroupExclusiveMin:            out.debug.append("subgroupExclusiveMin"); break;
    case EOpSubgroupExclusiveMax:            out.debug.append("subgroupExclusiveMax"); break;
    case EOpSubgroupExclusiveAnd:            out.debug.append("subgroupExclusiveAnd"); break;
    case EOpSubgroupExclusiveOr:             out.debug.append("subgroupExclusiveOr"); break;
    case EOpSubgroupExclusiveXor:            out.debug.append("subgroupExclusiveXor"); break;
    case EOpSubgroupClusteredAdd:            out.debug.append("subgroupClusteredAdd"); break;
    case EOpSubgroupClusteredMul:            out.debug.append("subgroupClusteredMul"); break;
    case EOpSubgroupClusteredMin:            out.debug.append("subgroupClusteredMin"); break;
    case EOpSubgroupClusteredMax:            out.debug.append("subgroupClusteredMax"); break;
    case EOpSubgroupClusteredAnd:            out.debug.append("subgroupClusteredAnd"); break;
    case EOpSubgroupClusteredOr:             out.debug.append("subgroupClusteredOr"); break;
    case EOpSubgroupClusteredXor:            out.debug.append("subgroupClusteredXor"); break;
    case EOpSubgroupQuadBroadcast:           out.debug.append("subgroupQuadBroadcast"); break;
    case EOpSubgroupQuadSwapHorizontal:      out.debug.append("subgroupQuadSwapHorizontal"); break;
    case EOpSubgroupQuadSwapVertical:        out.debug.append("subgroupQuadSwapVertical"); break;
    case EOpSubgroupQuadSwapDiagonal:        out.debug.append("subgroupQuadSwapDiagonal"); break;

    case EOpSubpassLoad:   out.debug.append("subpassLoad");   break;
    case EOpSubpassLoadMS: out.debug.append("subpassLoadMS"); break;

    default:
			   out.debug.append("ERROR: Bad aggregation op\n");
			   break;
    }

    if (node->getOp() != EOpSequence && node->getOp() != EOpParameters)
    {
	    out.debug.append(" (");
	    out.debug << node->getCompleteString();
	    out.debug.append(")");
    }

    out.debug.append("\n");

    return true;
}

bool TOutputTraverser::visitSelection(TVisit /* visit */, TIntermSelection* node)
{
    TInfoSink& out = infoSink;

    OutputTreeText(out, node, depth);

    out.debug.append("Test condition and select");
    out.debug.append(" (");
    out.debug << node->getCompleteString();
    out.debug.append(")");

    if (node->getShortCircuit() == false)
        out.debug.append(": no shortcircuit");
    if (node->getFlatten())
        out.debug.append(": Flatten");
    if (node->getDontFlatten())
        out.debug.append(": DontFlatten");
    out.debug.append("\n");

    ++depth;

    OutputTreeText(out, node, depth);
    out.debug.append("Condition\n");
    node->getCondition()->traverse(this);

    OutputTreeText(out, node, depth);
    if (node->getTrueBlock()) {
        out.debug.append("true case\n");
        node->getTrueBlock()->traverse(this);
    } else
        out.debug.append("true case is null\n");

    if (node->getFalseBlock()) {
        OutputTreeText(out, node, depth);
        out.debug.append("false case\n");
        node->getFalseBlock()->traverse(this);
    }

    --depth;

    return false;
}

// Print infinities and NaNs, and numbers in a portable way.
// Goals:
//   - portable (across IEEE 754 platforms)
//   - shows all possible IEEE values
//   - shows simple numbers in a simple way, e.g., no leading/trailing 0s
//   - shows all digits, no premature rounding
static void OutputDouble(TInfoSink& out, double value, TOutputTraverser::EExtraOutput extra)
{
    if (IsInfinity(value)) {
        if (value < 0)
            out.debug.append("-1.#INF");
        else
            out.debug.append("+1.#INF");
    } else if (IsNan(value))
        out.debug.append("1.#IND");
    else {
        const int maxSize = 340;
        char buf[maxSize];
        const char* format = "%f";
        if (fabs(value) > 0.0 && (fabs(value) < 1e-5 || fabs(value) > 1e12))
            format = "%-.13e";
        int len = snprintf(buf, maxSize, format, value);
        assert(len < maxSize);

        // remove a leading zero in the 100s slot in exponent; it is not portable
        // pattern:   XX...XXXe+0XX or XX...XXXe-0XX
        if (len > 5) {
            if (buf[len-5] == 'e' && (buf[len-4] == '+' || buf[len-4] == '-') && buf[len-3] == '0') {
                buf[len-3] = buf[len-2];
                buf[len-2] = buf[len-1];
                buf[len-1] = '\0';
            }
        }

        out.debug << buf;

        switch (extra) {
        case TOutputTraverser::BinaryDoubleOutput:
        {
            out.debug.append(" : ");
            long long b = *reinterpret_cast<long long*>(&value);
            for (size_t i = 0; i < 8 * sizeof(value); ++i, ++b) {
                out.debug << ((b & 0x8000000000000000) != 0 ? "1" : "0");
                b <<= 1;
            }
            break;
        }
        default:
            break;
        }
    }
}

static void OutputConstantUnion(TInfoSink& out, const TIntermTyped* node, const TConstUnionArray& constUnion,
    TOutputTraverser::EExtraOutput extra, int depth)
{
    int size = node->getType().computeNumComponents();

    for (int i = 0; i < size; i++) {
        OutputTreeText(out, node, depth);
        switch (constUnion[i].getType()) {
        case EbtBool:
            if (constUnion[i].getBConst())
                out.debug.append("true");
            else
                out.debug.append("false");

            out.debug.append(" (const bool)");

            out.debug.append("\n");
            break;
        case EbtFloat:
        case EbtDouble:
        case EbtFloat16:
            OutputDouble(out, constUnion[i].getDConst(), extra);
            out.debug.append("\n");
            break;
        case EbtInt8:
            {
                const int maxSize = 300;
                char buf[maxSize];
                snprintf(buf, maxSize, "%d (%s)", constUnion[i].getI8Const(), "const int8_t");

                out.debug << buf;
		out.debug.append("\n");
            }
            break;
        case EbtUint8:
            {
                const int maxSize = 300;
                char buf[maxSize];
                snprintf(buf, maxSize, "%u (%s)", constUnion[i].getU8Const(), "const uint8_t");

                out.debug << buf;
		out.debug.append("\n");
            }
            break;
        case EbtInt16:
            {
                const int maxSize = 300;
                char buf[maxSize];
                snprintf(buf, maxSize, "%d (%s)", constUnion[i].getI16Const(), "const int16_t");

                out.debug << buf;
		out.debug.append("\n");
            }
            break;
        case EbtUint16:
            {
                const int maxSize = 300;
                char buf[maxSize];
                snprintf(buf, maxSize, "%u (%s)", constUnion[i].getU16Const(), "const uint16_t");

                out.debug << buf;
		out.debug.append("\n");
            }
            break;
        case EbtInt:
            {
                const int maxSize = 300;
                char buf[maxSize];
                snprintf(buf, maxSize, "%d (%s)", constUnion[i].getIConst(), "const int");

                out.debug << buf;
		out.debug.append("\n");
            }
            break;
        case EbtUint:
            {
                const int maxSize = 300;
                char buf[maxSize];
                snprintf(buf, maxSize, "%u (%s)", constUnion[i].getUConst(), "const uint");

                out.debug << buf;
		out.debug.append("\n");
            }
            break;
        case EbtInt64:
            {
                const int maxSize = 300;
                char buf[maxSize];
                snprintf(buf, maxSize, "%lld (%s)", constUnion[i].getI64Const(), "const int64_t");

                out.debug << buf;
		out.debug.append("\n");
            }
            break;
        case EbtUint64:
            {
                const int maxSize = 300;
                char buf[maxSize];
                snprintf(buf, maxSize, "%llu (%s)", constUnion[i].getU64Const(), "const uint64_t");

                out.debug << buf;
		out.debug.append("\n");
            }
            break;
        default:
	    out.info.append("INTERNAL ERROR: ");
	    out.info.location(node->getLoc());
            out.info.append("Unknown constant\n");
            break;
        }
    }
}

void TOutputTraverser::visitConstantUnion(TIntermConstantUnion* node)
{
    OutputTreeText(infoSink, node, depth);
    infoSink.debug.append("Constant:\n");

    OutputConstantUnion(infoSink, node, node->getConstArray(), extraOutput, depth + 1);
}

void TOutputTraverser::visitSymbol(TIntermSymbol* node)
{
    OutputTreeText(infoSink, node, depth);

    infoSink.debug.append("'");
    infoSink.debug << node->getName() << "' (" << node->getCompleteString() << ")\n";

    if (! node->getConstArray().empty())
        OutputConstantUnion(infoSink, node, node->getConstArray(), extraOutput, depth + 1);
    else if (node->getConstSubtree()) {
        incrementDepth(node);
        node->getConstSubtree()->traverse(this);
        decrementDepth();
    }
}

bool TOutputTraverser::visitLoop(TVisit /* visit */, TIntermLoop* node)
{
    TInfoSink& out = infoSink;

    OutputTreeText(out, node, depth);

    out.debug.append("Loop with condition ");
    if (! node->testFirst())
        out.debug.append("not ");
    out.debug.append("tested first");

    if (node->getUnroll())
        out.debug.append(": Unroll");
    if (node->getDontUnroll())
        out.debug.append(": DontUnroll");
    if (node->getLoopDependency()) {
        out.debug.append(": Dependency ");
        out.debug << node->getLoopDependency();
    }
    out.debug.append("\n");

    ++depth;

    OutputTreeText(infoSink, node, depth);
    if (node->getTest()) {
        out.debug.append("Loop Condition\n");
        node->getTest()->traverse(this);
    } else
        out.debug.append("No loop condition\n");

    OutputTreeText(infoSink, node, depth);
    if (node->getBody()) {
        out.debug.append("Loop Body\n");
        node->getBody()->traverse(this);
    } else
        out.debug.append("No loop body\n");

    if (node->getTerminal()) {
        OutputTreeText(infoSink, node, depth);
        out.debug.append("Loop Terminal Expression\n");
        node->getTerminal()->traverse(this);
    }

    --depth;

    return false;
}

bool TOutputTraverser::visitBranch(TVisit /* visit*/, TIntermBranch* node)
{
    TInfoSink& out = infoSink;

    OutputTreeText(out, node, depth);

    switch (node->getFlowOp()) {
    case EOpKill:      out.debug.append("Branch: Kill");           break;
    case EOpBreak:     out.debug.append("Branch: Break");          break;
    case EOpContinue:  out.debug.append("Branch: Continue");       break;
    case EOpReturn:    out.debug.append("Branch: Return");         break;
    case EOpCase:      out.debug.append("case: ");                 break;
    case EOpDefault:   out.debug.append("default: ");              break;
    default:           out.debug.append("Branch: Unknown Branch"); break;
    }

    if (node->getExpression()) {
        out.debug.append(" with expression\n");
        ++depth;
        node->getExpression()->traverse(this);
        --depth;
    } else
        out.debug.append("\n");

    return false;
}

bool TOutputTraverser::visitSwitch(TVisit /* visit */, TIntermSwitch* node)
{
    TInfoSink& out = infoSink;

    OutputTreeText(out, node, depth);
    out.debug.append("switch");

    if (node->getFlatten())
        out.debug.append(": Flatten");
    if (node->getDontFlatten())
        out.debug.append(": DontFlatten");
    out.debug.append("\n");

    OutputTreeText(out, node, depth);
    out.debug.append("condition\n");
    ++depth;
    node->getCondition()->traverse(this);

    --depth;
    OutputTreeText(out, node, depth);
    out.debug.append("body\n");
    ++depth;
    node->getBody()->traverse(this);

    --depth;

    return false;
}

} // end namespace glslang
