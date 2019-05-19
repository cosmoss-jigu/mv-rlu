#include "global.h"
#include "helper.h"
#include "table.h"
#include "catalog.h"
#include "row.h"
#include "mem_alloc.h"
#if CC_ALG == MVRLU
#include "mvrlu.h"
#else
#include "rlu.h"
#endif

void table_t::init(Catalog * schema) {
	this->table_name = schema->table_name;
	this->schema = schema;
}

RC table_t::get_new_row(row_t *& row) {
	// this function is obsolete. 
	assert(false);
	return RCOK;
}

// the row is not stored locally. the pointer must be maintained by index structure.
RC table_t::get_new_row(row_t *& row, uint64_t part_id, uint64_t &row_id) {
	RC rc = RCOK;
	cur_tab_size ++;

#if CC_ALG == RLU || CC_ALG == MVRLU
        row = (row_t *)RLU_ALLOC(sizeof(row_t));
        assert(row != NULL);
#else
	row = (row_t *) _mm_malloc(sizeof(row_t), 64);
#endif
	rc = row->init(this, part_id, row_id);
	row->init_manager(row);

	return rc;
}
