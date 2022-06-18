#include <script/vm/VM.hpp>
#include <script/vm/Value.hpp>
#include <script/vm/HeapValue.hpp>
#include <script/vm/Array.hpp>
#include <script/vm/Slice.hpp>
#include <script/vm/Object.hpp>
#include <script/vm/ImmutableString.hpp>
#include <script/vm/TypeInfo.hpp>
#include <script/vm/InstructionHandler.hpp>

#include <types.h>
#include <script/Instructions.hpp>
#include <script/Hasher.hpp>
#include <system/debug.h>

#include <algorithm>
#include <cstdio>
#include <cinttypes>
#include <mutex>
#include <sstream>

namespace hyperion {
namespace vm {

HYP_FORCE_INLINE static void HandleInstruction(
    InstructionHandler &handler,
    BytecodeStream *bs,
    uint8_t code
)
{
    switch (code) {
    case STORE_STATIC_STRING: {
        // get string length
        uint32_t len; bs->Read(&len);

        // read string based on length
        char *str = new char[len + 1];
        bs->Read(str, len);
        str[len] = '\0';

        handler.StoreStaticString(
            len,
            str
        );

        delete[] str;

        break;
    }
    case STORE_STATIC_ADDRESS: {
        bc_address_t addr; bs->Read(&addr);

        handler.StoreStaticAddress(
            addr
        );

        break;
    }
    case STORE_STATIC_FUNCTION: {
        bc_address_t addr; bs->Read(&addr);
        uint8_t nargs; bs->Read(&nargs);
        uint8_t flags; bs->Read(&flags);

        handler.StoreStaticFunction(
            addr,
            nargs,
            flags
        );

        break;
    }
    case STORE_STATIC_TYPE: {
        uint16_t type_name_len; bs->Read(&type_name_len);

        char *type_name = new char[type_name_len + 1];
        type_name[type_name_len] = '\0';
        bs->Read(type_name, type_name_len);

        uint16_t size; bs->Read(&size);

        AssertThrow(size > 0);

        char **names = new char*[size];
        // load each name
        for (int i = 0; i < size; i++) {
            uint16_t length;
            bs->Read(&length);

            names[i] = new char[length + 1];
            names[i][length] = '\0';
            bs->Read(names[i], length);
        }

        handler.StoreStaticType(
            type_name,
            size,
            names
        );

        delete[] type_name;
        
        // delete the names
        for (size_t i = 0; i < size; i++) {
            delete[] names[i];
        }

        delete[] names;

        break;
    }
    case LOAD_I32: {
        bc_reg_t reg; bs->Read(&reg);
        int32_t i32; bs->Read(&i32);

        handler.LoadI32(
            reg,
            i32
        );

        break;
    }
    case LOAD_I64: {
        bc_reg_t reg; bs->Read(&reg);
        int64_t i64; bs->Read(&i64);

        handler.LoadI64(
            reg,
            i64
        );

        break;
    }
    case LOAD_U32: {
        bc_reg_t reg; bs->Read(&reg);
        uint32_t u32; bs->Read(&u32);

        handler.LoadU32(
            reg,
            u32
        );

        break;
    }
    case LOAD_U64: {
        bc_reg_t reg; bs->Read(&reg);
        uint64_t u64; bs->Read(&u64);

        handler.LoadU64(
            reg,
            u64
        );

        break;
    }
    case LOAD_F32: {
        bc_reg_t reg; bs->Read(&reg);
        float f32; bs->Read(&f32);

        handler.LoadF32(
            reg,
            f32
        );

        break;
    }
    case LOAD_F64: {
        bc_reg_t reg; bs->Read(&reg);
        double f64; bs->Read(&f64);

        handler.LoadF64(
            reg,
            f64
        );

        break;
    }
    case LOAD_OFFSET: {
        bc_reg_t reg; bs->Read(&reg);
        uint16_t offset; bs->Read(&offset);

        handler.LoadOffset(
            reg,
            offset
        );

        break;
    }
    case LOAD_INDEX: {
        bc_reg_t reg; bs->Read(&reg);
        uint16_t index; bs->Read(&index);

        handler.LoadIndex(
            reg,
            index
        );

        break;
    }
    case LOAD_STATIC: {
        bc_reg_t reg; bs->Read(&reg);
        uint16_t index; bs->Read(&index);

        handler.LoadStatic(
            reg,
            index
        );

        break;
    }
    case LOAD_STRING: {
        bc_reg_t reg; bs->Read(&reg);
        // get string length
        uint32_t len; bs->Read(&len);

        // read string based on length
        char *str = new char[len + 1];
        str[len] = '\0';
        bs->Read(str, len);

        handler.LoadString(
            reg,
            len,
            str
        );

        delete[] str;

        break;
    }
    case LOAD_ADDR: {
        bc_reg_t reg; bs->Read(&reg);
        bc_address_t addr; bs->Read(&addr);

        handler.LoadAddr(
            reg,
            addr
        );

        break;
    }
    case LOAD_FUNC: {
        bc_reg_t reg; bs->Read(&reg);
        bc_address_t addr; bs->Read(&addr);
        uint8_t nargs; bs->Read(&nargs);
        uint8_t flags; bs->Read(&flags);

        handler.LoadFunc(
            reg,
            addr,
            nargs,
            flags
        );

        break;
    }
    case LOAD_TYPE: {
        bc_reg_t reg; bs->Read(&reg);
        uint16_t type_name_len; bs->Read(&type_name_len);

        char *type_name = new char[type_name_len + 1];
        type_name[type_name_len] = '\0';
        bs->Read(type_name, type_name_len);

        // number of members
        uint16_t size;
        bs->Read(&size);

        char **names = new char*[size];
        
        // load each name
        for (int i = 0; i < size; i++) {
            uint16_t length;
            bs->Read(&length);

            names[i] = new char[length + 1];
            names[i][length] = '\0';
            bs->Read(names[i], length);
        }

        handler.LoadType(
            reg,
            type_name_len,
            type_name,
            size,
            names
        );

        delete[] type_name;
        
        // delete the names
        for (size_t i = 0; i < size; i++) {
            delete[] names[i];
        }

        delete[] names;

        break;
    }
    case LOAD_MEM: {
        bc_reg_t dst; bs->Read(&dst);
        bc_reg_t src; bs->Read(&src);
        uint8_t index; bs->Read(&index);

        handler.LoadMem(
            dst,
            src,
            index
        );

        break;
    }
    case LOAD_MEM_HASH: {
        bc_reg_t dst; bs->Read(&dst);
        bc_reg_t src; bs->Read(&src);
        uint32_t hash; bs->Read(&hash);

        handler.LoadMemHash(
            dst,
            src,
            hash
        );

        break;
    }
    case LOAD_ARRAYIDX: {
        bc_reg_t dst_reg; bs->Read(&dst_reg);
        bc_reg_t src_reg; bs->Read(&src_reg);
        bc_reg_t index_reg; bs->Read(&index_reg);

        handler.LoadArrayIdx(
            dst_reg,
            src_reg,
            index_reg
        );

        break;
    }
    case LOAD_REF: {
        bc_reg_t dst_reg;
        bc_reg_t src_reg;

        bs->Read(&dst_reg);
        bs->Read(&src_reg);

        handler.LoadRef(
            dst_reg,
            src_reg
        );
    }
    case LOAD_DEREF: {
        bc_reg_t dst_reg;
        bc_reg_t src_reg;

        bs->Read(&dst_reg);
        bs->Read(&src_reg);

        handler.LoadDeref(
            dst_reg,
            src_reg
        );
    }
    case LOAD_NULL: {
        bc_reg_t reg; bs->Read(&reg);

        handler.LoadNull(
            reg
        );

        break;
    }
    case LOAD_TRUE: {
        bc_reg_t reg; bs->Read(&reg);

        handler.LoadTrue(
            reg
        );

        break;
    }
    case LOAD_FALSE: {
        bc_reg_t reg; bs->Read(&reg);

        handler.LoadFalse(
            reg
        );

        break;
    }
    case MOV_OFFSET: {
        uint16_t offset; bs->Read(&offset);
        bc_reg_t reg; bs->Read(&reg);

        handler.MovOffset(
            offset,
            reg
        );

        break;
    }
    case MOV_INDEX: {
        uint16_t index; bs->Read(&index);
        bc_reg_t reg; bs->Read(&reg);

        handler.MovIndex(
            index,
            reg
        );

        break;
    }
    case MOV_MEM: {
        bc_reg_t dst; bs->Read(&dst);
        uint8_t index; bs->Read(&index);
        bc_reg_t src; bs->Read(&src);

        handler.MovMem(
            dst,
            index,
            src
        );

        break;
    }
    case MOV_MEM_HASH: {
        bc_reg_t dst; bs->Read(&dst);
        uint32_t hash; bs->Read(&hash);
        bc_reg_t src; bs->Read(&src);

        handler.MovMemHash(
            dst,
            hash,
            src
        );

        break;
    }
    case MOV_ARRAYIDX: {
        bc_reg_t dst; bs->Read(&dst);
        uint32_t index; bs->Read(&index);
        bc_reg_t src; bs->Read(&src);

        handler.MovArrayIdx(
            dst,
            index,
            src
        );

        break;
    }
    case MOV_ARRAYIDX_REG: {
        bc_reg_t dst; bs->Read(&dst);
        bc_reg_t index_reg; bs->Read(&index_reg);
        bc_reg_t src; bs->Read(&src);

        handler.MovArrayIdxReg(
            dst,
            index_reg,
            src
        );

        break;
    }
    case MOV_REG: {
        bc_reg_t dst; bs->Read(&dst);
        bc_reg_t src; bs->Read(&src);
        
        handler.MovReg(
            dst,
            src
        );

        break;
    }
    case HAS_MEM_HASH: {
        bc_reg_t dst; bs->Read(&dst);
        bc_reg_t src; bs->Read(&src);
        uint32_t hash; bs->Read(&hash);

        handler.HasMemHash(
            dst,
            src,
            hash
        );

        break;
    }
    case PUSH: {
        bc_reg_t reg; bs->Read(&reg);

        handler.Push(
            reg
        );

        break;
    }
    case POP: {
        handler.Pop();

        break;
    }
    case POP_N: {
        uint8_t n; bs->Read(&n);
        
        handler.PopN(
            n
        );

        break;
    }
    case PUSH_ARRAY: {
        bc_reg_t dst; bs->Read(&dst);
        bc_reg_t src; bs->Read(&src);

        handler.PushArray(
            dst,
            src
        );

        break;
    }
    case JMP: {
        //bc_reg_t reg; bs->Read(&reg);
        bc_address_t addr;
        bs->Read(&addr);

        handler.Jmp(
            addr
        );

        break;
    }
    case JE: {
        bc_address_t addr;
        bs->Read(&addr);

        handler.Je(
            addr
        );

        break;
    }
    case JNE: {
        bc_address_t addr;
        bs->Read(&addr);

        handler.Jne(
            addr
        );

        break;
    }
    case JG: {
        bc_address_t addr;
        bs->Read(&addr);

        handler.Jg(
            addr
        );

        break;
    }
    case JGE: {
        bc_address_t addr;
        bs->Read(&addr);

        handler.Jge(
            addr
        );

        break;
    }
    case CALL: {
        bc_reg_t reg; bs->Read(&reg);
        uint8_t nargs; bs->Read(&nargs);

        handler.Call(
            reg,
            nargs
        );

        break;
    }
    case RET: {
        handler.Ret();
        
        break;
    }
    case BEGIN_TRY: {
        bc_address_t catch_address;
        bs->Read(&catch_address);

        handler.BeginTry(
            catch_address
        );

        break;
    }
    case END_TRY: {
        handler.EndTry();

        break;
    }
    case NEW: {
        bc_reg_t dst; bs->Read(&dst);
        bc_reg_t src; bs->Read(&src);

        handler.New(
            dst,
            src
        );

        break;
    }
    case NEW_ARRAY: {
        bc_reg_t dst; bs->Read(&dst);
        uint32_t size; bs->Read(&size);

        handler.NewArray(
            dst,
            size
        );

        break;
    }
    case CMP: {
        bc_reg_t lhs_reg; bs->Read(&lhs_reg);
        bc_reg_t rhs_reg; bs->Read(&rhs_reg);

        handler.Cmp(
            lhs_reg,
            rhs_reg
        );

        break;
    }
    case CMPZ: {
        bc_reg_t reg; bs->Read(&reg);

        handler.CmpZ(
            reg
        );

        break;
    }
    case ADD: {
        bc_reg_t lhs_reg; bs->Read(&lhs_reg);
        bc_reg_t rhs_reg; bs->Read(&rhs_reg);
        bc_reg_t dst_reg; bs->Read(&dst_reg);

        handler.Add(
            lhs_reg,
            rhs_reg,
            dst_reg
        );

        break;
    }
    case SUB: {
        bc_reg_t lhs_reg; bs->Read(&lhs_reg);
        bc_reg_t rhs_reg; bs->Read(&rhs_reg);
        bc_reg_t dst_reg; bs->Read(&dst_reg);

        handler.Sub(
            lhs_reg,
            rhs_reg,
            dst_reg
        );

        break;
    }
    case MUL: {
        bc_reg_t lhs_reg; bs->Read(&lhs_reg);
        bc_reg_t rhs_reg; bs->Read(&rhs_reg);
        bc_reg_t dst_reg; bs->Read(&dst_reg);

        handler.Mul(
            lhs_reg,
            rhs_reg,
            dst_reg
        );

        break;
    }
    case DIV: {
        bc_reg_t lhs_reg; bs->Read(&lhs_reg);
        bc_reg_t rhs_reg; bs->Read(&rhs_reg);
        bc_reg_t dst_reg; bs->Read(&dst_reg);

        handler.Div(
            lhs_reg,
            rhs_reg,
            dst_reg
        );

        break;
    }
    case MOD: {
        bc_reg_t lhs_reg; bs->Read(&lhs_reg);
        bc_reg_t rhs_reg; bs->Read(&rhs_reg);
        bc_reg_t dst_reg; bs->Read(&dst_reg);

        handler.Mod(
            lhs_reg,
            rhs_reg,
            dst_reg
        );

        break;
    }
    case AND: {
        bc_reg_t lhs_reg; bs->Read(&lhs_reg);
        bc_reg_t rhs_reg; bs->Read(&rhs_reg);
        bc_reg_t dst_reg; bs->Read(&dst_reg);

        handler.And(
            lhs_reg,
            rhs_reg,
            dst_reg
        );

        break;
    }
    case OR: {
        bc_reg_t lhs_reg; bs->Read(&lhs_reg);
        bc_reg_t rhs_reg; bs->Read(&rhs_reg);
        bc_reg_t dst_reg; bs->Read(&dst_reg);

        handler.Or(
            lhs_reg,
            rhs_reg,
            dst_reg
        );

        break;
    }
    case XOR: {
        bc_reg_t lhs_reg; bs->Read(&lhs_reg);
        bc_reg_t rhs_reg; bs->Read(&rhs_reg);
        bc_reg_t dst_reg; bs->Read(&dst_reg);

        handler.Xor(
            lhs_reg,
            rhs_reg,
            dst_reg
        );

        break;
    }
    case SHL: {
        bc_reg_t lhs_reg; bs->Read(&lhs_reg);
        bc_reg_t rhs_reg; bs->Read(&rhs_reg);
        bc_reg_t dst_reg; bs->Read(&dst_reg);

        handler.Shl(
            lhs_reg,
            rhs_reg,
            dst_reg
        );

        break;
    }
    case SHR: {
        bc_reg_t lhs_reg; bs->Read(&lhs_reg);
        bc_reg_t rhs_reg; bs->Read(&rhs_reg);
        bc_reg_t dst_reg; bs->Read(&dst_reg);

        handler.Shr(
            lhs_reg,
            rhs_reg,
            dst_reg
        );

        break;
    }
    case NEG: {
        bc_reg_t reg; bs->Read(&reg);

        handler.Neg(
            reg
        );

        break;
    }
    case NOT: {
        bc_reg_t reg; bs->Read(&reg);

        handler.Not(
            reg
        );

        break;
    }
    case TRACEMAP: {
        uint32_t len; bs->Read(&len);

        uint32_t stringmap_count;
        bs->Read(&stringmap_count);

        Tracemap::StringmapEntry *stringmap = nullptr;

        if (stringmap_count != 0) {
            stringmap = new Tracemap::StringmapEntry[stringmap_count];

            for (uint32_t i = 0; i < stringmap_count; i++) {
                bs->Read(&stringmap[i].entry_type);
                bs->ReadZeroTerminatedString((char*)&stringmap[i].data);
            }
        }

        uint32_t linemap_count;
        bs->Read(&linemap_count);

        Tracemap::LinemapEntry *linemap = nullptr;

        if (linemap_count != 0) {
            linemap = new Tracemap::LinemapEntry[linemap_count];
            bs->ReadBytes((char*)linemap, sizeof(Tracemap::LinemapEntry) * linemap_count);
        }

        handler.state->m_tracemap.Set(std::move(stringmap), std::move(linemap));

        break;
    }
    case REM: {
        uint32_t len; bs->Read(&len);
        // just skip comment
        bs->Skip(len);

        break;
    }
    case EXPORT: {
        bc_reg_t reg; bs->Read(&reg);
        uint32_t hash; bs->Read(&hash);

        handler.ExportSymbol(
            reg,
            hash
        );

        break;
    }
    default: {
        int64_t last_pos = (int64_t)bs->Position() - sizeof(uint8_t);
        utf::printf(UTF8_CSTR("unknown instruction '%d' referenced at location: 0x%" PRIx64 "\n"), code, last_pos);
        // seek to end of bytecode stream
        bs->Seek(bs->Size());

        return;
    }
    }
}

VM::VM()
    : m_invoke_now_level(0)
{
    m_state.m_vm = non_owning_ptr<VM>(this);
    // create main thread
    m_state.CreateThread();
}

VM::~VM()
{
}

void VM::PushNativeFunctionPtr(NativeFunctionPtr_t ptr)
{
    Value sv;
    sv.m_type = Value::NATIVE_FUNCTION;
    sv.m_value.native_func = ptr;

    AssertThrow(m_state.GetMainThread() != nullptr);
    m_state.GetMainThread()->m_stack.Push(sv);
}

void VM::Invoke(
    InstructionHandler *handler,
    const Value &value,
    uint8_t nargs
)
{
    VMState *state = handler->state;
    ExecutionThread *thread = handler->thread;
    BytecodeStream *bs = handler->bs;

    AssertThrow(state != nullptr);
    AssertThrow(thread != nullptr);
    AssertThrow(bs != nullptr);

    if (value.m_type != Value::FUNCTION) {
        if (value.m_type == Value::NATIVE_FUNCTION) {
            Value **args = new Value*[nargs > 0 ? nargs : 1];

            int i = (int)thread->m_stack.GetStackPointer() - 1;
            for (int j = nargs - 1; j >= 0 && i >= 0; i--, j--) {
                args[j] = &thread->m_stack[i];
            }

            hyperion::sdk::Params params;
            params.handler = handler;
            params.args = args;
            params.nargs = nargs;

            // disable auto gc so no collections happen during a native function
            state->enable_auto_gc = false;

            // call the native function
            value.m_value.native_func(params);

            // re-enable auto gc
            state->enable_auto_gc = ENABLE_GC;

            delete[] args;

            return;
        } else if (value.m_type == Value::HEAP_POINTER) {
            if (value.m_value.ptr == nullptr) {
                state->ThrowException(
                    thread,
                    Exception::NullReferenceException()
                );
                return;
            } else if (Object *object = value.m_value.ptr->GetPointer<Object>()) {
                if (Member *member = object->LookupMemberFromHash(hash_fnv_1("$invoke"))) {
                    const int sp = (int)thread->m_stack.GetStackPointer();
                    const int args_start = sp - nargs;

                    if (nargs > 0) {
                        // shift over by 1 -- and insert 'self' to start of args
                        // make a copy of last item to not overwrite it
                        thread->m_stack.Push(thread->m_stack[sp - 1]);

                        for (size_t i = args_start; i < sp - 1; i++) {
                            thread->m_stack[i + 1] = thread->m_stack[i];
                        }
                        
                        // set 'self' object to start of args
                        thread->m_stack[args_start] = value;
                    } else {
                        thread->m_stack.Push(value);
                    }

                    VM::Invoke(
                        handler,
                        member->value,
                        nargs + 1
                    );

                    Value &top = thread->m_stack.Top();
                    AssertThrow(top.m_type == Value::FUNCTION_CALL);

                    // bookkeeping to remove the closure object
                    // normally, arguments are popped after the call is returned,
                    // rather than within the body
                    top.m_value.call.varargs_push--;

                    return;
                }
            }
        }

        char buffer[255];
        std::sprintf(
            buffer,
            "cannot invoke type '%s' as a function",
            value.GetTypeString()
        );

        state->ThrowException(
            thread,
            Exception(buffer)
        );

        return;
    }
    
    if ((value.m_value.func.m_flags & FunctionFlags::VARIADIC) && nargs < value.m_value.func.m_nargs - 1) {
        // if variadic, make sure the arg count is /at least/ what is required
        state->ThrowException(
            thread,
            Exception::InvalidArgsException(
                value.m_value.func.m_nargs,
                nargs,
                true
            )
        );
    } else if (!(value.m_value.func.m_flags & FunctionFlags::VARIADIC) && value.m_value.func.m_nargs != nargs) {
        state->ThrowException(
            thread,
            Exception::InvalidArgsException(
                value.m_value.func.m_nargs,
                nargs
            )
        );
    } else {
        Value previous_addr;
        previous_addr.m_type = Value::FUNCTION_CALL;
        previous_addr.m_value.call.varargs_push = 0;
        previous_addr.m_value.call.return_address = (uint32_t)bs->Position();

        if (value.m_value.func.m_flags & FunctionFlags::VARIADIC) {
            // for each argument that is over the expected size, we must pop it from
            // the stack and add it to a new array.
            int varargs_amt = nargs - value.m_value.func.m_nargs + 1;
            if (varargs_amt < 0) {
                varargs_amt = 0;
            }
            
            // set varargs_push value so we know how to get back to the stack size before.
            previous_addr.m_value.call.varargs_push = varargs_amt - 1;
            
            // allocate heap object
            HeapValue *hv = state->HeapAlloc(thread);
            AssertThrow(hv != nullptr);

            // create Array object to hold variadic args
            Array arr(varargs_amt);

            for (int i = varargs_amt - 1; i >= 0; i--) {
                // push to array
                arr.AtIndex(i, thread->GetStack().Top());
                thread->GetStack().Pop();
            }

            // assign heap value to our array
            hv->Assign(arr);

            Value array_value;
            array_value.m_type = Value::HEAP_POINTER;
            array_value.m_value.ptr = hv;

            hv->Mark();

            // push the array to the stack
            thread->GetStack().Push(array_value);
        }

        // push the address
        thread->GetStack().Push(previous_addr);
        // seek to the new address
        bs->Seek(value.m_value.func.m_addr);

        // increase function depth
        thread->m_func_depth++;
    }
}

void VM::InvokeNow(
    BytecodeStream *bs,
    const Value &value,
    uint8_t nargs
)
{
    auto *thread = m_state.MAIN_THREAD;
    const uint32_t original_function_depth = thread->m_func_depth;

    InstructionHandler handler(
        &m_state,
        thread,
        bs
    );

    Invoke(
        &handler,
        value,
        nargs
    );

    if (value.m_type == Value::FUNCTION) { // don't do this for native function calls
        uint8_t code;

        while (!bs->Eof()) {
            bs->Read(&code);
            
            HandleInstruction(
                handler,
                bs,
                code
            );

            if (handler.thread->GetExceptionState().HasExceptionOccurred()) {
                HandleException(&handler);

                if (!handler.state->good) {
                    break;
                }
            }

            if (code == RET) {
                if (thread->m_func_depth == original_function_depth) {
                    return;
                }
            }
        }
    }
}

void VM::CreateStackTrace(ExecutionThread *thread, StackTrace *out)
{
    const size_t max_stack_trace_size = sizeof(out->call_addresses) / sizeof(out->call_addresses[0]);

    for (size_t i = 0; i < max_stack_trace_size; i++) {
        out->call_addresses[i] = -1;
    }

    size_t num_recorded_call_addresses = 0;

    for (size_t sp = thread->m_stack.GetStackPointer(); sp != 0; sp--) {
        if (num_recorded_call_addresses >= max_stack_trace_size) {
            break;
        }

        const Value &top = thread->m_stack[sp - 1];

        if (top.m_type == Value::FUNCTION_CALL) {
            out->call_addresses[num_recorded_call_addresses++] = (int)top.m_value.call.return_address;
        }
    }
}

void VM::HandleException(InstructionHandler *handler)
{
    ExecutionThread *thread = handler->thread;
    BytecodeStream *bs = handler->bs;

    StackTrace stack_trace;
    CreateStackTrace(thread, &stack_trace);

    std::cout << "stack_trace = \n";

    for (int i = 0; i < sizeof(stack_trace.call_addresses) / sizeof(stack_trace.call_addresses[0]); i++) {
        if (stack_trace.call_addresses[i] == -1) {
            break;
        }
        
        std::cout << "\t" << std::hex << stack_trace.call_addresses[i] << "\n";
    }

    std::cout << "=====\n";

    if (thread->m_exception_state.m_try_counter > 0) {
        // handle exception
        thread->m_exception_state.m_try_counter--;

        Value *top = nullptr;
        while ((top = &thread->m_stack.Top())->m_type != Value::TRY_CATCH_INFO) {
            thread->m_stack.Pop();
        }

        // top should be exception data
        AssertThrow(top != nullptr && top->m_type == Value::TRY_CATCH_INFO);

        // jump to the catch block
        bs->Seek(top->m_value.try_catch_info.catch_address);
        // reset the exception flag
        thread->m_exception_state.m_exception_occured = false;

        // pop exception data from stack
        thread->m_stack.Pop();
    }
}

void VM::Execute(BytecodeStream *bs)
{
    AssertThrow(bs != nullptr);
    AssertThrow(m_state.GetNumThreads() != 0);

    InstructionHandler handler(
        &m_state,
        m_state.MAIN_THREAD,
        bs
    );

    uint8_t code;

    while (!bs->Eof()) {
        bs->Read(&code);
        
        HandleInstruction(
            handler,
            bs,
            code
        );

        if (handler.thread->GetExceptionState().HasExceptionOccurred()) {
            HandleException(&handler);

            if (!handler.state->good) {
                break;
            }
        }
    }
}

} // namespace vm
} // namespace hyperion
