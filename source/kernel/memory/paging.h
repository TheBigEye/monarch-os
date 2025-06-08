#ifndef _KERNEL_PAGING_H
#define _KERNEL_PAGING_H 1

#include "../../common/common.h"

#define PAGE_SIZE 4096

void initializePaging(void);
void memoryPagingMap(uint32_t virtual, uint32_t physical);

#endif /* _KERNEL_PAGING_H */
