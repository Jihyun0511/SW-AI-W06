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
#define CHUNKSIZE (1 << 12) // sbrk로 4KB(1페이지)씩 요청

// 반올림 (비트 마스킹, 수식보다 더 빠름!)
#define ALIGN(size) (((size) + (DSIZE - 1)) & ~0x7)

// size, alloc 통합해서 리턴
#define PACK(size, alloc) ((size) | (alloc))

// 포인터
#define GET(p) (*(unsigned int *)(p))              // p가 참조하는 워드 리턴
#define PUT(p, val) (*(unsigned int *)(p) = (val)) // p가 참조하는 워드에 val 저장

// p의 size, alloc 리턴
#define GET_SIZE(p) (GET(p) & ~0x7) // 끝 세자리 빼고 뽑아온다. size 리턴
#define GET_ALLOC(p) (GET(p) & 0x1) // 끝자리만 뽑아온다. alloc 리턴

// 포인터들 리턴
#define HDRP(bp) ((char *)(bp) - WSIZE)                      // 헤더 포인터 리턴
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) // 푸터 포인터 리턴

// 앞뒤 블록들 포인터 리턴
// 내 포인터에서 헤더(WSIZE) 만큼 빼면 전체 블록 크기. 내 포인터 + 구한 사이즈 = 다음 블록 포인터
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
// 내 포인터에서 내 헤더 + 이전 푸터 (DSIZE) 만큼 빼면 이전 푸터 자리. 내 포인터 - 이전 블록 크기 = 이전 블록 포인터
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

// #define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

// 힙 메모리 포인터 선언
static char *heap_listp = 0;
// fit ptr 선언
static char *fit_ptr;

// 내부 함수 선언
static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);

/*
    가장 먼저 불려서 초기화 (힙 영역 할당 등)
    문제 발생 시 -1 / 아닐 시 0 반환
 */
int mm_init(void)
{
    // 초기 공간을 할당한다. 가져올 수 없으면 -1 반환
    heap_listp = mem_sbrk(4 * WSIZE);
    if (heap_listp == (void *)-1)
        return -1; // 에러 뜨면...

    // 초기 블록 만들기용 세팅
    PUT(heap_listp, 0); // 패딩 만들기
    // 프롤로그 헤더
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1));
    // 프롤로그 푸터
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1));
    // 에필로그 헤더
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));
    // 포인터를 bp 위치(블록의 데이터 시작점: 프롤로그 헤더/푸터 중간점)로 옮김
    heap_listp += (2 * WSIZE);

    // CHUNKSIZE만큼 초기 블록을 만든다
    char *initBp;
    initBp = extend_heap(CHUNKSIZE / WSIZE);
    // 추가 공간 할당. 실패 시 -1 반환
    if (initBp == NULL) {
        return -1; // 에러 뜨면...
    }

    fit_ptr = heap_listp;
    return 0;
}

/*
    최소 SIZE 바이트 크기의 할당된 블록 payload의 포인터 반환(8 바이트 정렬)
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;
    size_t extendize;
    char *bp;

    if (size == 0) return NULL;

    if (size <= DSIZE)
        asize = 2*DSIZE;
    else
        asize = ALIGN(size + DSIZE);
    
    //맞는 공간 찾기
    bp = find_fit(asize);

    // 맞는 공간 있음
    if (bp != NULL)
    {
        place(bp, asize);
        return bp;
    }

    // 맞는 공간 없음
    extendize = asize > CHUNKSIZE ? asize : CHUNKSIZE; // 늘릴 크기 정하기
    bp = extend_heap(extendize/WSIZE); // 힙 늘리기
    if(bp == NULL) return NULL; // 공간 없으면 NULL

    place(bp, asize); // 블록 배치
    return bp;
}

/*
    ptr이 가리키는 블록 해제 / 반환값 없음
    prt이 malloc이나 realloc으로 받은 것이고 해제되지 않았을 때에만 정상 작동
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
    // 반환하려는 블록 크기 확인
    size_t size = GET_SIZE(HDRP(bp));
    
    PUT(HDRP(bp), PACK(size, 0)); // 헤더 alloc 0으로
    PUT(FTRP(bp), PACK(size, 0)); // 푸터 alloc 0으로
    coalesce(bp); // 병합
}

/*
    - prt이 NULL이면, mm_malloc(size)와 동일
    - size가 0이면, mm_free(ptr)와 동일
    - ptr이 NULL이 아니라면, malloc이나 realloc을 통해 반환된 포인터여야 함.
      ptr이 가리키는 블록을 size 바이트로 변경하고, 새 블록의 주소 반환.

 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */

 /*
 void *mm_realloc(void *ptr, size_t size){
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;

    // 1. size가 0이면 free와 동일하게 동작
    if (size == 0) {
        mm_free(ptr);
        return NULL;
    }

    // 2. ptr이 NULL이면 malloc과 동일하게 동작
    if (ptr == NULL) {
        return mm_malloc(size);
    }

    // 3. 새로운 공간 할당
    newptr = mm_malloc(size);
    if (newptr == NULL)
        return NULL;

    // 4. 이전 데이터 복사 (핵심!)
    // GET_SIZE로 원래 블록의 전체 크기를 구한 뒤, 헤더/푸터 크기(DSIZE)를 빼서 순수 데이터 크기만 복사
    copySize = GET_SIZE(HDRP(oldptr)) - DSIZE; 
    
    if (size < copySize)
        copySize = size; // 만약 더 작은 크기로 줄이는 거라면 넘치지 않게 자름
    
    memcpy(newptr, oldptr, copySize); // 새 공간으로 데이터 이사
    
    // 5. 이사 끝났으니 기존 방 빼기
    mm_free(oldptr);
    
    return newptr;
}
*/

void *mm_realloc(void *ptr, size_t size)
{
    if (size == 0) {
        mm_free(ptr);
        return NULL;
    }
    if (ptr == NULL) {
        return mm_malloc(size);
    }

    void *newptr;
    size_t old_size = GET_SIZE(HDRP(ptr));
    
    // 1. 새로 요청한 크기를 블록 정렬 기준에 맞춤 (malloc의 asize 계산과 동일)
    size_t asize;
    if (size <= DSIZE) asize = 2 * DSIZE;
    else asize = ALIGN(size + DSIZE);

    // 2. 이미 기존 방이 충분히 크다면? 아무것도 안 하고 그냥 그대로 살면 됨
    if (asize <= old_size) {
        return ptr;
    }

    // 3. 방을 넓혀야 하는데, 마침 바로 다음 블록이 비어있고 둘을 합치면 크기가 충분한가?
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(ptr)));
    size_t next_size = GET_SIZE(HDRP(NEXT_BLKP(ptr)));

    if (!next_alloc && (old_size + next_size >= asize)) {
        // 벽을 허물고 두 방을 하나로 합침 (헤더와 푸터 갱신)
        size_t combined_size = old_size + next_size;
        PUT(HDRP(ptr), PACK(combined_size, 1));
        PUT(FTRP(ptr), PACK(combined_size, 1));
        
        // 🚨 Next Fit 전용 방어 코드: 하필 fit_ptr이 먹혀버린 다음 블록을 가리키고 있었다면 안전한 곳으로 대피
        if (fit_ptr == NEXT_BLKP(ptr)) {
            fit_ptr = ptr; 
        }
        
        return ptr; // 이사 가지 않고 그대로 반환
    }

    // 4. 옆방도 사용 중이거나 합쳐도 좁다면, 어쩔 수 없이 눈물을 머금고 새 방을 구해 이사
    newptr = mm_malloc(size);
    if (newptr == NULL) return NULL;
    
    memcpy(newptr, ptr, old_size - DSIZE);
    mm_free(ptr);
    
    return newptr;
}

// 메모리 확장
static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    // 정렬 맞추기용 반올림
    size = ALIGN(words * WSIZE);

    // 힙 공간 요청. 메모리 꽉 차서 실패하면(-1) NULL 반환.
    bp = mem_sbrk(size);
    if ((long)bp == -1)
        return NULL; // C는 형변환 할 때 타입에 괄호 씌워야 한다

    // 기존 에필로그 블록 헤더 -> 새 헤더
    PUT(HDRP(bp), PACK(size, 0));
    // 푸터 생성
    PUT(FTRP(bp), PACK(size, 0));
    // 새 블록 마지막 워드 -> 새 에필로그 헤더
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

    // 가용 블록 병합
    return coalesce(bp);
}

// 메모리 병합
static void *coalesce(void *bp)
{
    // 앞뒤 블록의 할당 여부 확인
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))); // 이전 블록의 푸터 alloc
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp))); // 다음 블록의 헤터 alloc

    size_t size = GET_SIZE(HDRP(bp));

    // 1. 이전, 다음 블록 할당 상태 / 병합 X
    if (prev_alloc && next_alloc) return bp;
    // 2. 이전 블록 할당, 다음 블록 가용 상태 / 다음 블록만 병합
    else if (prev_alloc && !next_alloc) {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp))); // 다음 블록 사이즈 더함
        // FRTP는 HDRP를 기준으로 찾기 때문에 HDRP 먼저 해야 함
        PUT(HDRP(bp), PACK(size, 0)); // 헤더 할당 플래그 0으로
        PUT(FTRP(bp), PACK(size, 0)); // 푸터 할당 플래그 0으로 (커진 사이즈 기준으로 맨 끝으로 간다)
    }
    // 3. 이전 블록 가용, 다음 블록 할당 상태 / 이전 블록만 병합
    else if (!prev_alloc && next_alloc) {
        size += GET_SIZE(HDRP(PREV_BLKP(bp))); // 이전 블록 사이즈 더함
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0)); // 이전 블록 헤더 가져옴
        bp = PREV_BLKP(bp); // 이전 블록 포인터로 갱신
    }
    // 4. 이전, 다음 블록 가용 상태 / 이전, 다음 블록 모두 병합
    else {
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0)); // 이전 블록 헤더 가져옴
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0)); // 다음 블록 푸터 가져옴
        bp = PREV_BLKP(bp);
    }

    // 합쳐진 블록 안에 rover가 갇혔다면, rover를 새 블록의 시작점(bp)으로 구출!
    if ((fit_ptr > (char *)bp) && (fit_ptr < NEXT_BLKP(bp))) {
        fit_ptr = bp;
    }

    return bp;
}

// 묵시적 가용 리스트에서의 next fit 검색
static void *find_fit(size_t asize)
{
    void *bp = fit_ptr; // 기존 포인터값 기억해두기
    
    // fit_ptr 부터 끝까지 검색
    for(; GET_SIZE(HDRP(fit_ptr)) != 0; fit_ptr = NEXT_BLKP(fit_ptr))
    {
        // 가용 블록인지 확인 / 블록 사이즈 확인 (나보다 크거나 같은지)
        if (!GET_ALLOC(HDRP(fit_ptr)) && (asize <= GET_SIZE(HDRP(fit_ptr))))
            return fit_ptr;
    }

    // 끝까지 갔는데 없으면 처음부터 기존 fit_ptr 자리까지 검색
    for(fit_ptr = heap_listp; fit_ptr < bp; fit_ptr = NEXT_BLKP(fit_ptr))
    {
        // 가용 블록인지 확인 / 블록 사이즈 확인 (나보다 크거나 같은지)
        if (!GET_ALLOC(HDRP(fit_ptr)) && (asize <= GET_SIZE(HDRP(fit_ptr))))
            return fit_ptr;
    }

    // 맞는 곳 없음
    return NULL;
}

/*
    요청한 블록을 가용 블록의 시작 부분에 배치
    나머지 부분의 크기가 최소 블록 크기와 같거나 큰 경우에만 분할한다
*/
static void place(void *bp, size_t asize)
{
    // 빈공간 사이즈 확인
    size_t csize = GET_SIZE(HDRP(bp));

    // 공간 충분? 나눠주고도 충분히 남는가?
    if ((csize - asize) >= (2*DSIZE))
    {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));
    }
    // 공간이 없으면 그냥 다 주기
    else
    {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}

