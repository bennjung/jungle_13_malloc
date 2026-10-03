/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 *
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

// 매크로 리스트 
#define WSIZE 4  // header, footer size (single word == 4B)
#define DSIZE 8 // double word size == Minimum block size (8B) 
#define CHUNKSIZE (1<<12)  // 24bytes 

#define MAX(x, y) ((x) > (y) ? (x) : (y))

// 블록 헤더 만들기(사이즈 + 할당 상태(a,f))
#define PACK(size, alloc) ((size) | (alloc)) 

// 할당 포인터 주소 읽기, 삽입
#define GET(p)     (*(unsigned int *)(p)) 
#define PUT(p,val) (*(unsigned int *)(p) = (val))

// 현재 포인터에서 할당 크기 읽기 
#define GET_SIZE(p)  (GET(p) & ~0x7) // 0x7(0000_0111) Not => 1111_1000 비트 마스킹 
#define GET_ALLOC(p) (GET(p) & 0x1) // 0x1(0000_0001) And => Allocated? Free? 

// 블록 포인터 -> 블록 내부 header, footer 주소 계산 
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp))- WSIZE * 2)

// 블록 포인터 -> prev, next 블록 주소 계산
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE((char *)(bp) - DSIZE))

static char *heap_listp;

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 4

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))
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
    ""
};

static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);
/*
 * mm_init - initialize the malloc package.
 */


int mm_init(void){
    // 0. 초기 힙 생성
    // 예외처리 sbrk? WSIZE = 4 
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *) - 1) return -1;

    // 1. 패딩 설정 
    PUT(heap_listp, 0); // unused block
    PUT(heap_listp +(1*WSIZE), PACK(DSIZE, 1)); // prologue header
    PUT(heap_listp +(2*WSIZE), PACK(DSIZE, 1)); // prologue footer (header copy)
    PUT(heap_listp +(3*WSIZE), PACK(0, 1)); // epilogue header(0B, alc)
    heap_listp += (2*WSIZE); // initial alloc point(address)

    
    // 2. 새로운 힙 가져오기?
    // mem_sbrk(CHUNKSIZE);
    
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL){
        return -1;
    } 
    // extend_heap(1<<18);

    return 0;
}

// 메모리 할당 
static void place(void *bp, size_t asize){
    // 인자로 free block 주소가 들어옴
    // Dsize 이상이면 스플릿? 왜지요? 내부 단편화 막기위해서? 
    // 스플릿 해주는 이유 새로운 free block area 만들어야하니까.
    
    // 0. 프리 사이즈에서 요청 사이즈 뺸 크기가 DSIZE 보다 크거나 같음. 
    size_t temp = GET_SIZE(HDRP(bp));

    // 1.헤더의 값을 바꾸기 
    PUT(HDRP(bp), PACK(asize, 1));
    
    // 2. 푸터 바꿔주기
    PUT(bp+asize -(WSIZE*2), PACK(asize, 1));

    // 3. 무적권 8 초과
    if (temp > asize){
        PUT(bp+asize-WSIZE, PACK(temp-asize,0));
        PUT(bp+temp-DSIZE, PACK(temp-asize,0));
        
        
    }
    

    return;
    

}
// 메모리 자리 찾기 
static void *find_fit(size_t asize){
    // first fit 
    // asize 이하의 첫번째로 찾은 블록 위치를 반환해줘야하는거 아님? 
    // 0. header, footer 플래그 확인하기 
    char *bp = heap_listp;
    while (GET_SIZE(bp) != 0){

        if (!GET_ALLOC(bp) && GET_SIZE(bp) <= asize){
            return bp;
        }

        bp = NEXT_BLKP(bp); 
    }

    
    return NULL;
    
}

// 새로운 힙 가져오기 (확장)
static void *extend_heap(size_t words){
    char *bp;
    size_t size;

    // word size 의 배수 만큼 할당 
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1) {
        return NULL;
    } // 예외처리 sbrk가 어떤 함수지요? 

    PUT(HDRP(bp), PACK(size, 0)); // Header in free block 
    PUT(FTRP(bp), PACK(size, 0)); // Footer in free block 
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); // init -> epilogue header

    // 이전 힙과 새로운 힙 병합
    return coalesce(bp);
    // bp = heap_listp;
    // PUT(bp, PACK(8000, 0));
    // PUT(bp+(8000-4), PACK(8000, 0));
    // // return 1;
    // return bp;
    

}




/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size){
    
    size_t asize;
    size_t esize;
    char *bp;

    if (size == 0) return NULL;

    // 1. 블록 사이즈 맞추기 (더블 워드)
    if (size <= DSIZE) asize = WSIZE*2+DSIZE;
    // size = 10
    else {
        // asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);
        if (DSIZE+size % 8 ) asize = DSIZE * ((DSIZE+size )/ 8);
        else asize = DSIZE * (((DSIZE+size) / 8) +1);
        
    }

    // 2. 블록 할당 위치 진짜 있음?
    if ((bp = find_fit(asize)) != NULL){
        
        place(bp, asize);
        return bp;
    }

    // 2.1 블록 위치 못 찾음 -> 새로운 힙 배치
    esize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(esize/WSIZE)) == NULL) return NULL;
    place(bp, asize);
    return bp;
    

}

static void *coalesce(void *bp){
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) { /* Case 1 */
        return bp;
    }

    else if (prev_alloc && !next_alloc) { /* Case 2 */
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size,0));
    }

    else if (!prev_alloc && next_alloc) { /* Case 3 */
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    else { /* Case 4 */
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    return bp;
}


//Freeing a block does nothing.
void mm_free(void *bp){
    size_t size = GET_SIZE(HDRP(bp));

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp);
}

//Implemented simply in terms of mm_malloc and mm_free
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;

    newptr = mm_malloc(size);
    if (newptr == NULL)
        return NULL;
    copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    if (size < copySize)
        copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}