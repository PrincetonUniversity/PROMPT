enum UnifiedAction : char {
  INIT = 0,
  LOAD,
  STORE,
  ALLOC,
  FREE,
  TARGET_LOOP_INVOC,
  TARGET_LOOP_ITER,
  TARGET_LOOP_EXIT,
  LOOP_ENTRY,
  LOOP_EXIT,
  LOOP_ITER_CTX,
  FUNC_ENTRY,
  FUNC_EXIT,
  POINTS_TO_INST,
  POINTS_TO_ARG,
  FINISHED
};
using Action = UnifiedAction;
#define CONSUME_QUEUE_DEFINE()
#define CONSUME_QUEUE_INIT()
#define CONSUME_QUEUE_DEBUG()

#define CONSUME_PACKET(action)
#define CONSUME_INIT(loop_id, pid)
#define CONSUME_ALLOC(inst_id, size, addr)
#define CONSUME_FREE(addr)
#define CONSUME_FINISHED()
#define CONSUME_FUNC_ENTRY(fn_id)
#define CONSUME_FUNC_EXIT(fn_id)
#define CONSUME_LOOP_ENTRY(loop_id)
#define CONSUME_LOOP_EXIT(loop_id)
#define CONSUME_LOOP_ITER_CTX(loop_id)
#define CONSUME_TARGET_LOOP_INVOC()
#define CONSUME_TARGET_LOOP_ITER()
#define CONSUME_TARGET_LOOP_EXIT()
#define CONSUME_POINTS_TO_ARG(fn_arg_id, addr)
#define CONSUME_POINTS_TO_INST(inst_id, addr)
#define CONSUME_LOAD(instr, addr, value)
#define CONSUME_STORE(instr, addr)
