#include "txn.h"
#include "row.h"
#include "manager.h"
#include "row_rlu.h"
#include "mem_alloc.h"
#include <mm_malloc.h>
#include "thread.h"

#if CC_ALG == RLU || CC_ALG == MVRLU
#define CPU_RELAX() asm volatile("pause\n": : :"memory");
void Row_rlu::init(row_t *row){
        _row = row;
        return;
}

RC Row_rlu::access(txn_man *txn, TsType type, row_t *row)
{
        RC rc = RCOK;
        rlu_thread_data_t* self = txn->h_thd->p_rlu_td;
        if (type == R_REQ){
                txn->cur_row = (row_t*) RLU_DEREF(self, _row);
                return rc;
        } else if(type == P_REQ){
                row_t *update_row = _row;
                if(!RLU_TRY_LOCK(self, &update_row)){
                        rc = Abort;
                        return rc;
                }
                txn->cur_row = update_row;
                return rc;
        } else
                return rc;
                

}
#endif
