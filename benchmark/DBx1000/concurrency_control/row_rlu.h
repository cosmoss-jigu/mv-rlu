#pragma once

#if CC_ALG == RLU
#include "rlu.h"
#endif
#if CC_ALG == MVRLU
#include "mvrlu.h"
#endif

class table_t; class Catalog;
class txn_man;
#if CC_ALG == RLU || CC_ALG == MVRLU
class Row_rlu{
public:
        void init(row_t *row);
        RC access(txn_man *txn, TsType type, row_t *row);
private:
        row_t * _row;

};
#endif
