 /* MALLOC LAB::IMPLICIT */ 

 /*
    memlib.c 패키지는 동적 메모리 할당기를 위한 메모리 시스템을 시뮬레이션합니다.
    memlib.c에서 다음 함수들을 호출할 수 있습니다.   
    void *mem_sbrk(int incr): incr 바이트만큼 힙을 확장합니다.
        여기서 incr은 양의 0이 아닌 정수여야 하며, 새로 할당된 힙 영역의 첫 번째 바이트를 가리키는 제네릭 포인터를 반환합니다.
        mem_sbrk가 양의 정수 인수만 허용한다는 점을 제외하면 Unix sbrk 함수와 동일한 의미론을 가집니다.  
    void *mem_heap_lo(void): 힙의 첫 번째 바이트를 가리키는 제네릭 포인터를 반환합니다. 
    void *mem_heap_hi(void): 힙의 마지막 바이트를 가리키는 제네릭 포인터를 반환합니다. 
    size_t mem_heapsize(void): 현재 힙의 크기를 바이트 단위로 반환합니다.   
    size_t mem_pagesize(void): 시스템의 페이지 크기를 바이트 단위로 반환합니다 (Linux 시스템에서는 4K). 
*/
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "ateam",
    /* First member's full name */
    "Harry Bovik",
    /* First member's email address */
    "bovik@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""};

/* single word (4) or double word (8) alignment */
#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1<<12) // sbrk로 4KB(1페이지)씩 요청

// 반올림 (비트 마스킹, 수식보다 더 빠름!)
#define ALIGN(size) (((size) + (DSIZE - 1)) & ~0x7)

// size, alloc 통합해서 리턴
#define PACK(size, alloc) ((size) | (alloc))

// 포인터
#define GET(p)      (*(unsigned int *)(p)) // p가 참조하는 워드 리턴
#define PUT(p, val) (*(unsigned int *)(p) = (val)) // p가 참조하는 워드에 val 저장

// p의 size, alloc 리턴
#define GET_SIZE(p)  (GET(p) & ~0x7) // 끝 세자리 빼고 뽑아온다. size 리턴
#define GET_ALLOC(p) (GET(p) & 0x1) // 끝자리만 뽑아온다. alloc 리턴

// 포인터들 리턴
#define HDRP(bp)    ((char *)(bp) - WSIZE) // 헤더 포인터 리턴
#define FTRP(bp)    ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) // 푸터 포인터 리턴

// 앞뒤 블록들 포인터 리턴
// 내 포인터에서 헤더(WSIZE) 만큼 빼면 전체 블록 크기. 내 포인터 + 구한 사이즈 = 다음 블록 포인터
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
// 내 포인터에서 내 헤더 + 이전 푸터 (DSIZE) 만큼 빼면 이전 푸터 자리. 내 포인터 - 이전 블록 크기 = 이전 블록 포인터
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

// #define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

// 힙 메모리 포인터 선언
static char *heap_listp = 0;

/*
    가장 먼저 불려서 초기화 (힙 영역 할당 등)
    문제 발생 시 -1 / 아닐 시 0 반환
 */
int mm_init(void)
{
    // 초기 공간을 할당한다. 가져올 수 없으면 -1 반환
    heap_listp = mem_sbrk(4*WSIZE);
    if (heap_listp == (void *)-1) return -1; // 에러 뜨면...

    // 초기 블록 만들기용 세팅
    PUT(heap_listp, 0); // 패딩 만들기
    // 프롤로그 헤더
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1));
    // 프롤로그 푸터
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1));
    // 에필로그 헤더
    PUT(heap_listp + (3*WSIZE), PACK(0, 1));
    // 포인터를 bp 위치(블록의 데이터 시작점: 프롤로그 헤더/푸터 중간점)로 옮김
    heap_listp += (2*WSIZE);

    // CHUNKSIZE만큼 초기 블록을 만든다
    

    return 0;
}

static void *extend_heap(size_t words)
{
    
}

/*
    최소 SIZE 바이트 크기의 할당된 블록 payload의 포인터 반환(8 바이트 정렬)
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    // int newsize = ALIGN(size + SIZE_T_SIZE);
    // void *p = mem_sbrk(newsize);
    // if (p == (void *)-1)
    //     return NULL;
    // else
    // {
    //     *(size_t *)p = size;
    //     return (void *)((char *)p + SIZE_T_SIZE);
    // }
}

/*
    ptr이 가리키는 블록 해제 / 반환값 없음
    prt이 malloc이나 realloc으로 받은 것이고 해제되지 않았을 때에만 정상 작동
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
}

/*
    - prt이 NULL이면, mm_malloc(size)와 동일
    - size가 0이면, mm_free(ptr)와 동일
    - ptr이 NULL이 아니라면, malloc이나 realloc을 통해 반환된 포인터여야 함.
      ptr이 가리키는 블록을 size 바이트로 변경하고, 새 블록의 주소 반환.

 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    // void *oldptr = ptr;
    // void *newptr;
    // size_t copySize;

    // newptr = mm_malloc(size);
    // if (newptr == NULL)
    //     return NULL;
    // copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    // if (size < copySize)
    //     copySize = size;
    // memcpy(newptr, oldptr, copySize);
    // mm_free(oldptr);
    // return newptr;
}