#include "srcxx/intercept_routing_handler.h"
#include "srcxx/ThreadSupport.h"

void pre_call_forward_handler(RegisterContext *reg_ctx, HookEntry *entry) {

  // run the `pre_call` before execute origin function which has been relocated(fixed)
  if (entry->pre_call) {
    PRECALL pre_call;
    HookEntryInfo entry_info;
    entry_info.hook_id        = entry->id;
    entry_info.target_address = entry->target_address;
    pre_call                  = entry->pre_call;

    StackFrame *stackframe = new StackFrame;
    // create stack frame as common variable between pre_call and post_call
    ThreadSupport::PushStackFrame(stackframe);

    // run the pre_call with the power of accessing all registers
    (*pre_call)(reg_ctx, &entry_info);
  }

  // set the prologue bridge next hop address with the patched instructions has been relocated
  set_prologue_routing_next_hop(reg_ctx, entry->relocated_origin_function);

  // replace the function ret address with our epilogue_routing_dispatch
  set_func_ret_address(reg_ctx, entry->epilogue_dispatch_bridge);
}

void post_call_forward_handler(RegisterContext *reg_ctx, HookEntry *entry) {

  // run the `post_call`, and access all the register value, as the origin function done,
  if (entry->post_call) {
    POSTCALL post_call;
    HookEntryInfo entry_info;
    entry_info.hook_id        = entry->id;
    entry_info.target_address = entry->target_address;
    post_call                 = entry->post_call;

    // run the post_call with the power of accessing all registers
    (*post_call)(reg_ctx, (const HookEntryInfo *)&entry_info);
  }

  // pop stack frame as common variable between pre_call and post_call
  StackFrame *stackframe = ThreadSupport::PopStackFrame();

  // set epilogue bridge next hop address with origin ret address, restore the call.
  set_epilogue_routing_next_hop(reg_ctx, stackframe->orig_ret);
}

void dynamic_binary_instrumentation_call_forward_handler(RegisterContext *reg_ctx, HookEntry *entry) {

  // run the `dbi_call`, before the `instruction_address`
  if (entry->dbi_call) {
    DBICALL dbi_call;
    HookEntryInfo entry_info;
    entry_info.hook_id             = entry->id;
    entry_info.instruction_address = entry->instruction_address;
    dbi_call                       = entry->dbi_call;
    (*dbi_call)(reg_ctx, (const HookEntryInfo *)&entry_info);
  }

  // set prologue bridge next hop address with origin instructions that have been relocated(patched)
  set_prologue_routing_next_hop(reg_ctx, entry->relocated_origin_instructions);
}

void prologue_routing_dispatch(RegisterContext *reg_ctx, ClosureTrampolineEntry *closure_trampoline_entry) {
  HookEntry *entry = static_cast<HookEntry *>(closure_trampoline_entry->carry_data);
  if (entry->type == kFunctionWrapper)
    pre_call_forward_handler(reg_ctx, entry);
  else if (entry->type == kDynamicBinaryInstrumentation)
    dynamic_binary_instrumentation_call_forward_handler(reg_ctx, entry);
  return;
}

void epilogue_routing_dispatch(RegisterContext *reg_ctx, ClosureTrampolineEntry *closure_trampoline_entry) {
  HookEntry *entry = static_cast<HookEntry *>(closure_trampoline_entry->carry_data);
  post_call_forward_handler(reg_ctx, entry);
  return;
}

void intercept_routing_common_bridge_handler(RegisterContext *reg_ctx, ClosureTrampolineEntry *entry) {
  USER_CODE_CALL UserCodeCall = (USER_CODE_CALL)entry->carry_hanlder;
  UserCodeCall(reg_ctx, entry);
  return;
}