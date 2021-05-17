# Milestone 1 : Expand the maximum size of a file

### bmap 함수
<br>
* 다이렉트 블록은 10개 까지 가능합니다.
* 11번째 블록에서는 1 level indirect 블록을 사용합니다.
* 12번째 블록에서는 2 level indirect 블록을 사용합니다.
* 13번째 블록에서는 3 level indirect 블록을 사용합니다.
* 비 대칭적 트리 구조를 통해 작은 파일들은 대부분 direct 방식을 통해 해결합니다.

#### some constants
<br>
```c
#define NDIRECT 10
#define NINDIRECT (BSIZE / sizeof(uint))
#define NDINDIRECT (NINDIRECT * NINDIRECT)
#define NTINDIRECT (NINDIRECT * NINDIRECT * NINDIRECT)
#define MAXFILE (NDIRECT + NINDIRECT + NDINDIRECT + NTINDIRECT)
```
>
* 레벨이 늘어났기 때문에 그에 맞게 바꾸어줍니다.



#### Doubly indirect
```c
  bn -= NINDIRECT;
  if (bn < NDINDIRECT) {
    int offset = NDIRECT+1;
    uint first_table_offset = bn / NINDIRECT;
    uint second_table_offset = bn % NINDIRECT;

    if((addr = ip->addrs[offset]) ==0)
      ip->addrs[offset] = addr = balloc(ip->dev);
```
>
* offset은 아이노드 테이블 상의 위치를 가리키는 용도입니다.
* first_table_offset은 offset이 가리키는 블락에서의 위치를 가리키는 용도입니다.
* second_table_offset은 first_table_offset이 가리키는 블락에서의 위치를 가리키는 용도입니다.
* 아이노드의 offset위치에 블락을 할당시킵니다.

<br>

```c
    //first table setting
    bp = bread(ip->dev, addr);
    a = (uint*)bp->data;
    if((addr = a[first_table_offset]) == 0) {
      a[first_table_offset] = addr = balloc(ip->dev);
      log_write(bp);
    }
    brelse(bp);

    //second table setting
    bp = bread(ip->dev, addr);
    a = (uint*)bp->data;
    if((addr = a[second_table_offset]) == 0) {
      a[second_table_offset] = addr = balloc(ip->dev);
      log_write(bp);
    }
    brelse(bp);
    return addr;
  }
```
>
* 실제 데이터가 존재하는 블락까지의 패쓰에 존재하는 블락들을 할당 시켜줍니다.
<br>

#### Triple indirect

```c
    int offset = NDIRECT+2;
    uint first_table_offset = bn / NDINDIRECT;
    uint second_table_offset = bn / NINDIRECT;
    uint third_table_offset = bn % NINDIRECT;
```
>
* 3 level indirect를 구현하기 위해 필요한 offset 들입니다.

<br>
```c
    //first table setting
    bp = bread(ip->dev, addr);
    a = (uint*)bp->data;

    if((addr = a[first_table_offset]) == 0) {
      a[first_table_offset] = addr = balloc(ip->dev);
      log_write(bp);
    }
    brelse(bp);


    //second table setting
    bp = bread(ip->dev, addr);
    a = (uint*)bp -> data;

    if((addr = a[second_table_offset]) ==0) {
      a[second_table_offset] = addr = balloc(ip->dev);
      log_write(bp);
    }
    brelse(bp);

    //third table setting
    bp = bread(ip->dev, addr);
    a = (uint*)bp -> data;

    if((addr = a[third_table_offset]) == 0) {
      a[third_table_offset] = addr = balloc(ip->dev);
      log_write(bp);
    }
    brelse(bp);

    return addr;
  }

```
>
* 2 level indirect와 방식은 동일합니다.

### itrunc 함수
<br>
* 할당된 블록을 해제해주는 역할입니다.
* n-level indirect로 할당된 블록들은 맨 끝에 데이터부터 순차적으로 해제해줍니다.

#### free indirect
```c
  //remove doubly indirect
  offset = NDIRECT+1;
  if(ip->addrs[offset]){
    bp = bread(ip->dev, ip->addrs[offset]);
    a = (uint*)bp -> data;
    for(i = 0; i < NINDIRECT; i++) {
      if(a[i]) {
        bp2 = bread(ip->dev, a[i]);
        b = (uint*)bp2->data;
        for(j = 0 ; j<NINDIRECT; j++){
          if(b[j])
            bfree(ip->dev, b[j]);
        }
        brelse(bp2);
        bfree(ip->dev, a[i]);
      }
    }
    brelse(bp);
    bfree(ip->dev, ip->addrs[offset]);
    ip->addrs[offset] = 0;
  }

  //remove triple indirect
  offset = NDIRECT+2;
  if(ip->addrs[offset]){
    bp = bread(ip->dev, ip->addrs[offset]);
    a = (uint*)bp ->data;

    for(i = 0 ; i< NINDIRECT; i++) {
      if(a[i]) {
        bp2 = bread(ip->dev, a[i]);
        b = (uint*)bp2->data;
        for(j = 0 ; j<NINDIRECT; j++){
          if(b[j]){
            bp3 = bread(ip->dev, b[j]);
            c = (uint*)bp3->data;
            for(k = 0 ; k<NINDIRECT; k++){
              if(c[k])
                bfree(ip->dev, c[k]);
            }
            brelse(bp3);
            bfree(ip->dev, b[j]);
          }
        }
      brelse(bp2);
      bfree(ip->dev,a[i]);
      }
    }

  brelse(bp);
  bfree(ip->dev, ip->addrs[offset]);
  ip->addrs[offset] = 0;

  }
```
>
* 방식 자체는 bmap에서의 할당 과정과 매우 유사합니다.


# Milestone 2 : Implement the sync system call

### end_op 함수
<br>
* 원래는 end_op가 호출될 때마다 디스크에 write를 수행하는 방식이었는데, 이제는 sync를 호출하지 않는다면 버퍼가 꽉 찬 경우에만 write를 수행하도록 변경하였습니다.

```c
  #define MAXOPBLOCKS  12

...

  if(log.lh.n + (log.outstanding+1) *MAXOPBLOCKS >LOGSIZE) {
    do_commit = 1;      
    log.committing = 1;
  }  
```
>
* commit되는 조건을 해당 식으로 변경함으로써 버퍼가 꽉 찬 경우에만 commit을 수행하도록 하였습니다.
* MAXOPBLOCKS의 값을 10에서 12로 변경하였는데, 이 이유는 indirect 레벨이 높아짐에 maxopblocks를 초과하는 만큼의 블록에 write가 되는 경우가 간헐적으로 존재하기 때문에, 값을 높여주었습니다.

### sync 함수
```c
int sync(void)
{
  commit();
  acquire(&log.lock);
  log.committing = 0 ;
  wakeup(&log);
  release(&log.lock);
  return 0;
}
```
>
* 호출할  경우 commit이 이루어집니다. 

### get_log_num
```c
int get_log_num(void)
{
  return log.lh.n;
}
```
>
* 명세에 적힌 그대로 log.lh.n을 반환합니다.


### 결과분석

#### test.hugefile.c

![1](/uploads/20c421ffe191f162d02cb5dc0f09f8f3/1.png)

* 매우 잘 돌아가는 것을 확인할 수 있습니다. ( 너무 길어 부분만 올리겠습니다.)

#### sync 확인

![testsync만들고직후](/uploads/d8bc0f529b31d111a53334545645a9ad/testsync만들고직후.png)
* 2 byte를 쓰고 (synctest) 바로 ls한 결과입니다.
* c파일에 sync()를 사용해주지 않고 xv6를 종료한다면 다음 실행에 해당 파일은 없어집니다.
* c파일에 sync()를 사용해주고 xv6를 종료한다면 다음 실행에도 해당 파일은 여전히 존재합니다.


#### 고친 에러
![no_buffers](/uploads/62bff52c407d32c95897426130351cae/no_buffers.png)

* stress test로 넘어가는 부분에 unlink에서 나는 에러입니다.
* MAXOPBLOCKS을 10에서 12로 늘려주지 않는 경우에 간헐적으로 해당 오류가 발생하는 것을 확인할 수 있었습니다.  (현재 고쳤습니다.)
































































