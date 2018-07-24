#ifndef NOCC_RTX_OCC_H_
#define NOCC_RTX_OCC_H_

#include "all.h"

#include "tx_operator.hpp"
#include "logger.hpp"

#include "core/rworker.h"
#include "framework/view_manager.h"

#include <vector>

namespace nocc {

using namespace oltp;

namespace rtx {

class RtxOCC : public TXOpBase {
#include "rtx_occ_internal.h" // common data structures definition
 public:
  // nid: local node id. If == -1, all operations go through the network
  // resposne_node == nid: enable local accesses.
  // response_node == -1, all local operations go through network
  RtxOCC(oltp::RWorker *worker,MemDB *db,RRpc *rpc_handler,int nid,int cid,int response_node);

  // provide a hook to init RDMA based contents, using TXOpBase
  RtxOCC(oltp::RWorker *worker,MemDB *db,RRpc *rpc_handler,int nid,int tid,int cid,int response_node,
         RdmaCtrl *cm,RDMA_sched *sched,int ms):
      TXOpBase(worker,db,rpc_handler,cm,sched,response_node,tid,ms),// response_node shall always equal *real node id*
      read_batch_helper_(rpc_->get_static_buf(MAX_MSG_SIZE),reply_buf_),
      write_batch_helper_(rpc_->get_static_buf(MAX_MSG_SIZE),reply_buf_),
      cor_id_(cid),response_node_(nid)
  {

  }

  void set_logger(Logger *log) { logger_ = log; }

  // start a TX
  virtual void begin(yield_func_t &yield);

  // commit a TX
  virtual bool commit(yield_func_t &yield);

  template <int tableid,typename V> // the value stored corresponding to tableid
  int  add_to_read(int pid,uint64_t key,yield_func_t &yield);

  // directly add the record to the write-set
  template <int tableid,typename V> // the value stored corresponding to tableid
  int  add_to_write(int pid,uint64_t key,yield_func_t &yield);

  template <typename V>
  V *get_readset(int idx,yield_func_t &yield);

  template <typename V>
  V *get_writeset(int idx,yield_func_t &yield);

  template <int tableid,typename V>
  V *get(int pid,uint64_t key,yield_func_t &yield) {
    int idx = add_to_read<tableid,V>(pid,key,yield);
    return get_readset<V>(idx,yield);
  }

  template <int tableid,typename V>
  int insert(int pid,uint64_t key,V *val,yield_func_t &yield);

  // add a specific item in read-set to writeset
  int     add_to_write(int idx);
  int     add_to_write();

  virtual int      local_read(int tableid,uint64_t key,int len,yield_func_t &yield);
  virtual int      local_insert(int tableid,uint64_t key,char *val,int len,yield_func_t &yield);
  virtual int      remote_read(int pid,int tableid,uint64_t key,int len,yield_func_t &yield);
  virtual int      remote_insert(int pid,int tableid,uint64_t key,int len,yield_func_t &yield);

  // if local, the batch_get will return the results
  virtual void     start_batch_read();
  virtual int      add_batch_read(int tableid,uint64_t key,int pid,int len);
  virtual int      add_batch_insert(int tableid,uint64_t key,int pid,int len);
  virtual int      add_batch_write(int tableid,uint64_t key,int pid,int len);
  virtual int      send_batch_read(int idx = 0);
  virtual bool     parse_batch_result(int num);

  virtual void gc_readset();
  virtual void gc_writeset();

  virtual bool lock_writes(yield_func_t &yield);
  virtual bool release_writes(yield_func_t &yield);
  virtual bool validate_reads(yield_func_t &yield);
  virtual void write_back(yield_func_t &yield);
  virtual void log_remote(yield_func_t &yield);

 protected:
  std::vector<ReadSetItem>  read_set_;
  std::vector<ReadSetItem>  write_set_;  // stores the index of readset

  // helper to send batch read/write operations
  BatchOpCtrlBlock read_batch_helper_;
  BatchOpCtrlBlock write_batch_helper_;

  const int cor_id_;
  const int response_node_;

  Logger *logger_       = NULL;

  bool abort_ = false;
  char reply_buf_[MAX_MSG_SIZE];

  // helper functions
  void register_default_rpc_handlers();

 private:
  // RPC handlers
  void read_rpc_handler(int id,int cid,char *msg,void *arg);
  void lock_rpc_handler(int id,int cid,char *msg,void *arg);
  void release_rpc_handler(int id,int cid,char *msg,void *arg);
  void commit_rpc_handler(int id,int cid,char *msg,void *arg);
  void validate_rpc_handler(int id,int cid,char *msg,void *arg);
 protected:
  void prepare_write_contents();

  DISABLE_COPY_AND_ASSIGN(RtxOCC);
};

};
}; // namespace nocc

// RPC ids
#define RTX_READ_RPC_ID 0
#define RTX_LOCK_RPC_ID 1
#define RTX_RELEASE_RPC_ID 2
#define RTX_COMMIT_RPC_ID  3
#define RTX_VAL_RPC_ID     4
#define RTX_LOG_RPC_ID     5
#define RTX_LOG_CLEAN_ID   6

#include "rtx_occ_impl.hpp"

#endif
