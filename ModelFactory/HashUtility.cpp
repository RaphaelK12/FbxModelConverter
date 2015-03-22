#include "StdAfx.h"
#include "HashUtility.h"

#include <stdio.h>
#include <stdint.h>

#include <assert.h>
 
 //�ֽ����Сͷ�ʹ�ͷ������
 #define HASH_LITTLE_ENDIAN  0x0123
 #define HASH_BIG_ENDIAN     0x3210
 
 //Ŀǰ���еĴ��붼��Ϊ��Сͷ������ģ���֪������֮�����״����Ƿ񻹻�Ϊ��ͷ������һ�Σ�
 #ifndef HASH_BYTES_ORDER
 #define HASH_BYTES_ORDER    HASH_LITTLE_ENDIAN
 #endif
 
 #ifndef HASH_SWAP_UINT16
 #define HASH_SWAP_UINT16(x)  ((((x) & 0xff00) >>  8) | (((x) & 0x00ff) <<  8))
 #endif
 #ifndef HASH_SWAP_UINT32
 #define HASH_SWAP_UINT32(x)  ((((x) & 0xff000000) >> 24) | (((x) & 0x00ff0000) >>  8) | \
     (((x) & 0x0000ff00) <<  8) | (((x) & 0x000000ff) << 24))
 #endif
 #ifndef HASH_SWAP_UINT64
 #define HASH_SWAP_UINT64(x)  ((((x) & 0xff00000000000000) >> 56) | (((x) & 0x00ff000000000000) >>  40) | \
     (((x) & 0x0000ff0000000000) >> 24) | (((x) & 0x000000ff00000000) >>  8) | \
     (((x) & 0x00000000ff000000) << 8 ) | (((x) & 0x0000000000ff0000) <<  24) | \
     (((x) & 0x000000000000ff00) << 40 ) | (((x) & 0x00000000000000ff) <<  56))
 #endif
 
 //��һ�����ַ��������飬����������һ��uint32_t���飬ͬʱÿ��uint32_t���ֽ���
 void *swap_uint32_memcpy(void *to, const void *from, size_t length)
 {
     memcpy(to, from, length);
     size_t remain_len =  (4 - (length & 3)) & 3;
 
     //���ݲ���4�ֽڵı���,����0
     if (remain_len)
     {
         for (size_t i = 0; i < remain_len; ++i)
         {
             *((char *)(to) + length + i) = 0;
         }
         //������4�ı���
         length += remain_len;
     }
 
     //���е����ݷ�ת
     for (size_t i = 0; i < length / 4; ++i)
     {
         ((uint32_t *)to)[i] = HASH_SWAP_UINT32(((uint32_t *)to)[i]);
     }
 
     return to;
 }
 
 //================================================================================================
 //MD5���㷨
 
 //ÿ�δ����BLOCK�Ĵ�С
 static const size_t HASH_MD5_BLOCK_SIZE = 64;
 
 //md5�㷨�������ģ�����һЩ״̬���м����ݣ����
 typedef struct md5_ctx
 {
     //��������ݵĳ���
     uint64_t length_;
     //��û�д�������ݳ���
     uint64_t unprocessed_;
     //ȡ�õ�HASH������м����ݣ�
     uint32_t  hash_[4];
 } md5_ctx;
 
 
 #define ROTL32(dword, n) ((dword) << (n) ^ ((dword) >> (32 - (n))))
 #define ROTR32(dword, n) ((dword) >> (n) ^ ((dword) << (32 - (n))))
 #define ROTL64(qword, n) ((qword) << (n) ^ ((qword) >> (64 - (n))))
 #define ROTR64(qword, n) ((qword) >> (n) ^ ((qword) << (64 - (n))))
 
 
 /*!
 @brief      �ڲ���������ʼ��MD5��context������
 @param      ctx
 */
 static void zen_md5_init(md5_ctx *ctx)
 {
     ctx->length_ = 0;
     ctx->unprocessed_ = 0;
 
     /* initialize state */
     ctx->hash_[0] = 0x67452301;
     ctx->hash_[1] = 0xefcdab89;
     ctx->hash_[2] = 0x98badcfe;
     ctx->hash_[3] = 0x10325476;
 }
 
 /* First, define four auxiliary functions that each take as input
  * three 32-bit words and returns a 32-bit word.*/
 
 /* F(x,y,z) = ((y XOR z) AND x) XOR z - is faster then original version */
 #define MD5_F(x, y, z) ((((y) ^ (z)) & (x)) ^ (z))
 #define MD5_G(x, y, z) (((x) & (z)) | ((y) & (~z)))
 #define MD5_H(x, y, z) ((x) ^ (y) ^ (z))
 #define MD5_I(x, y, z) ((y) ^ ((x) | (~z)))
 
 /* transformations for rounds 1, 2, 3, and 4. */
 #define MD5_ROUND1(a, b, c, d, x, s, ac) { \
         (a) += MD5_F((b), (c), (d)) + (x) + (ac); \
         (a) = ROTL32((a), (s)); \
         (a) += (b); \
     }
 #define MD5_ROUND2(a, b, c, d, x, s, ac) { \
         (a) += MD5_G((b), (c), (d)) + (x) + (ac); \
         (a) = ROTL32((a), (s)); \
         (a) += (b); \
     }
 #define MD5_ROUND3(a, b, c, d, x, s, ac) { \
         (a) += MD5_H((b), (c), (d)) + (x) + (ac); \
         (a) = ROTL32((a), (s)); \
         (a) += (b); \
     }
 #define MD5_ROUND4(a, b, c, d, x, s, ac) { \
         (a) += MD5_I((b), (c), (d)) + (x) + (ac); \
         (a) = ROTL32((a), (s)); \
         (a) += (b); \
     }
 
 
 /*!
 @brief      �ڲ���������64���ֽڣ�16��uint32_t���������ժҪ���Ӵգ���������������Լ�����Сͷ����
 @param      state ��Ŵ����hash���ݽ��
 @param      block Ҫ�����block��64���ֽڣ�16��uint32_t������
 */
 static void zen_md5_process_block(uint32_t state[4], const uint32_t block[HASH_MD5_BLOCK_SIZE / 4])
 {
     register unsigned a, b, c, d;
     a = state[0];
     b = state[1];
     c = state[2];
     d = state[3];
 
     const uint32_t *x = NULL;
 
     //MD5�����������ݶ���Сͷ����.��ͷ��������Ҫ����
 #if HASH_BYTES_ORDER == HASH_LITTLE_ENDIAN
     x = block;
 #else
     uint32_t swap_block[HASH_MD5_BLOCK_SIZE / 4];
     swap_uint32_memcpy(swap_block, block, 64);
     x = swap_block;
 #endif
 
 
     MD5_ROUND1(a, b, c, d, x[ 0],  7, 0xd76aa478);
     MD5_ROUND1(d, a, b, c, x[ 1], 12, 0xe8c7b756);
     MD5_ROUND1(c, d, a, b, x[ 2], 17, 0x242070db);
     MD5_ROUND1(b, c, d, a, x[ 3], 22, 0xc1bdceee);
     MD5_ROUND1(a, b, c, d, x[ 4],  7, 0xf57c0faf);
     MD5_ROUND1(d, a, b, c, x[ 5], 12, 0x4787c62a);
     MD5_ROUND1(c, d, a, b, x[ 6], 17, 0xa8304613);
     MD5_ROUND1(b, c, d, a, x[ 7], 22, 0xfd469501);
     MD5_ROUND1(a, b, c, d, x[ 8],  7, 0x698098d8);
     MD5_ROUND1(d, a, b, c, x[ 9], 12, 0x8b44f7af);
     MD5_ROUND1(c, d, a, b, x[10], 17, 0xffff5bb1);
     MD5_ROUND1(b, c, d, a, x[11], 22, 0x895cd7be);
     MD5_ROUND1(a, b, c, d, x[12],  7, 0x6b901122);
     MD5_ROUND1(d, a, b, c, x[13], 12, 0xfd987193);
     MD5_ROUND1(c, d, a, b, x[14], 17, 0xa679438e);
     MD5_ROUND1(b, c, d, a, x[15], 22, 0x49b40821);
 
     MD5_ROUND2(a, b, c, d, x[ 1],  5, 0xf61e2562);
     MD5_ROUND2(d, a, b, c, x[ 6],  9, 0xc040b340);
     MD5_ROUND2(c, d, a, b, x[11], 14, 0x265e5a51);
     MD5_ROUND2(b, c, d, a, x[ 0], 20, 0xe9b6c7aa);
     MD5_ROUND2(a, b, c, d, x[ 5],  5, 0xd62f105d);
     MD5_ROUND2(d, a, b, c, x[10],  9,  0x2441453);
     MD5_ROUND2(c, d, a, b, x[15], 14, 0xd8a1e681);
     MD5_ROUND2(b, c, d, a, x[ 4], 20, 0xe7d3fbc8);
     MD5_ROUND2(a, b, c, d, x[ 9],  5, 0x21e1cde6);
     MD5_ROUND2(d, a, b, c, x[14],  9, 0xc33707d6);
     MD5_ROUND2(c, d, a, b, x[ 3], 14, 0xf4d50d87);
     MD5_ROUND2(b, c, d, a, x[ 8], 20, 0x455a14ed);
     MD5_ROUND2(a, b, c, d, x[13],  5, 0xa9e3e905);
     MD5_ROUND2(d, a, b, c, x[ 2],  9, 0xfcefa3f8);
     MD5_ROUND2(c, d, a, b, x[ 7], 14, 0x676f02d9);
     MD5_ROUND2(b, c, d, a, x[12], 20, 0x8d2a4c8a);
 
     MD5_ROUND3(a, b, c, d, x[ 5],  4, 0xfffa3942);
     MD5_ROUND3(d, a, b, c, x[ 8], 11, 0x8771f681);
     MD5_ROUND3(c, d, a, b, x[11], 16, 0x6d9d6122);
     MD5_ROUND3(b, c, d, a, x[14], 23, 0xfde5380c);
     MD5_ROUND3(a, b, c, d, x[ 1],  4, 0xa4beea44);
     MD5_ROUND3(d, a, b, c, x[ 4], 11, 0x4bdecfa9);
     MD5_ROUND3(c, d, a, b, x[ 7], 16, 0xf6bb4b60);
     MD5_ROUND3(b, c, d, a, x[10], 23, 0xbebfbc70);
     MD5_ROUND3(a, b, c, d, x[13],  4, 0x289b7ec6);
     MD5_ROUND3(d, a, b, c, x[ 0], 11, 0xeaa127fa);
     MD5_ROUND3(c, d, a, b, x[ 3], 16, 0xd4ef3085);
     MD5_ROUND3(b, c, d, a, x[ 6], 23,  0x4881d05);
     MD5_ROUND3(a, b, c, d, x[ 9],  4, 0xd9d4d039);
     MD5_ROUND3(d, a, b, c, x[12], 11, 0xe6db99e5);
     MD5_ROUND3(c, d, a, b, x[15], 16, 0x1fa27cf8);
     MD5_ROUND3(b, c, d, a, x[ 2], 23, 0xc4ac5665);
 
     MD5_ROUND4(a, b, c, d, x[ 0],  6, 0xf4292244);
     MD5_ROUND4(d, a, b, c, x[ 7], 10, 0x432aff97);
     MD5_ROUND4(c, d, a, b, x[14], 15, 0xab9423a7);
     MD5_ROUND4(b, c, d, a, x[ 5], 21, 0xfc93a039);
     MD5_ROUND4(a, b, c, d, x[12],  6, 0x655b59c3);
     MD5_ROUND4(d, a, b, c, x[ 3], 10, 0x8f0ccc92);
     MD5_ROUND4(c, d, a, b, x[10], 15, 0xffeff47d);
     MD5_ROUND4(b, c, d, a, x[ 1], 21, 0x85845dd1);
     MD5_ROUND4(a, b, c, d, x[ 8],  6, 0x6fa87e4f);
     MD5_ROUND4(d, a, b, c, x[15], 10, 0xfe2ce6e0);
     MD5_ROUND4(c, d, a, b, x[ 6], 15, 0xa3014314);
     MD5_ROUND4(b, c, d, a, x[13], 21, 0x4e0811a1);
     MD5_ROUND4(a, b, c, d, x[ 4],  6, 0xf7537e82);
     MD5_ROUND4(d, a, b, c, x[11], 10, 0xbd3af235);
     MD5_ROUND4(c, d, a, b, x[ 2], 15, 0x2ad7d2bb);
     MD5_ROUND4(b, c, d, a, x[ 9], 21, 0xeb86d391);
 
     state[0] += a;
     state[1] += b;
     state[2] += c;
     state[3] += d;
 }
 
 
 /*!
 @brief      �ڲ��������������ݵ�ǰ�沿��(>64�ֽڵĲ���)��ÿ�����һ��64�ֽڵ�block�ͽ����Ӵմ���
 @param[out] ctx  �㷨��context�����ڼ�¼һЩ����������ĺͽ��
 @param[in]  buf  ��������ݣ�
 @param[in]  size ��������ݳ���
 */
 static void zen_md5_update(md5_ctx *ctx, const unsigned char *buf, size_t size)
 {
     //Ϊʲô����=����Ϊ��ĳЩ�����£����Զ�ε���zen_md5_update����������������뱣֤ǰ��ĵ��ã�ÿ�ζ�û��unprocessed_
     ctx->length_ += size;
 
     //ÿ������Ŀ鶼��64�ֽ�
     while (size >= HASH_MD5_BLOCK_SIZE)
     {
         zen_md5_process_block(ctx->hash_, reinterpret_cast<const uint32_t *>(buf));
         buf  += HASH_MD5_BLOCK_SIZE;
         size -= HASH_MD5_BLOCK_SIZE;
     }
 
     ctx->unprocessed_ = size;
 }
 
 
 /*!
 @brief      �ڲ��������������ݵ�ĩβ���֣�����Ҫƴ�����1��������������Ҫ�����BLOCK������0x80�����ϳ��Ƚ��д���
 @param[in]  ctx    �㷨��context�����ڼ�¼һЩ����������ĺͽ��
 @param[in]  buf    ���������
 @param[in]  size   ����buffer�ĳ���
 @param[out] result ���صĽ����
 */
 static void zen_md5_final(md5_ctx *ctx, const unsigned char *buf, size_t size, unsigned char *result)
 {
     uint32_t message[HASH_MD5_BLOCK_SIZE / 4];
 
     //����ʣ������ݣ�����Ҫƴ�����1��������������Ҫ����Ŀ飬ǰ����㷨��֤�ˣ����һ����϶�С��64���ֽ�
     if (ctx->unprocessed_)
     {
         memcpy(message, buf + size - ctx->unprocessed_, static_cast<size_t>( ctx->unprocessed_));
     }
 
     //�õ�0x80Ҫ����ڵ�λ�ã���uint32_t �����У���
     uint32_t index = ((uint32_t)ctx->length_ & 63) >> 2;
     uint32_t shift = ((uint32_t)ctx->length_ & 3) * 8;
 
     //���0x80��ȥ�����Ұ����µĿռ䲹��0
     message[index]   &= ~(0xFFFFFFFF << shift);
     message[index++] ^= 0x80 << shift;
 
     //������block���޷����������ĳ����޷����ɳ���64bit����ô�ȴ������block
     if (index > 14)
     {
         while (index < 16)
         {
             message[index++] = 0;
         }
 
         zen_md5_process_block(ctx->hash_, message);
         index = 0;
     }
 
     //��0
     while (index < 14)
     {
         message[index++] = 0;
     }
 
     //���泤�ȣ�ע����bitλ�ĳ���,����������ҿ��������˰��죬
     uint64_t data_len = (ctx->length_) << 3;
 
     //ע��MD5�㷨Ҫ���64bit�ĳ�����СͷLITTLE-ENDIAN���룬ע������ıȽ���!=
 #if HASH_BYTES_ORDER != HASH_LITTLE_ENDIAN
     data_len = HASH_SWAP_UINT64(data_len);
 #endif
 
     message[14] = (uint32_t) (data_len & 0x00000000FFFFFFFF);
     message[15] = (uint32_t) ((data_len & 0xFFFFFFFF00000000ULL) >> 32);
 
     zen_md5_process_block(ctx->hash_, message);
 
     //ע������Сͷ���ģ��ڴ�ͷ������Ҫ����ת��
 #if HASH_BYTES_ORDER == HASH_LITTLE_ENDIAN
     memcpy(result, &ctx->hash_, MD5_HASH_SIZE);
 #else
     swap_uint32_memcpy(result, &ctx->hash_, MD5_HASH_SIZE);
 #endif
 
 }
 
 
 //����һ���ڴ����ݵ�MD5ֵ
 unsigned char* HashUtility::md5(const unsigned char *buf,
                             size_t size,
                             unsigned char result[MD5_HASH_SIZE])
 {
     assert(result != NULL);
 
     md5_ctx ctx;
     zen_md5_init(&ctx);
     zen_md5_update(&ctx, buf, size);
     zen_md5_final(&ctx, buf, size, result);
     return result;
 }
 
 
 
 
 //================================================================================================
 //SHA1���㷨
 
 //ÿ�δ����BLOCK�Ĵ�С
 static const size_t HASH_SHA1_BLOCK_SIZE = 64;
 
 //SHA1�㷨�������ģ�����һЩ״̬���м����ݣ����
 typedef struct sha1_ctx
 {
 
     //��������ݵĳ���
     uint64_t length_;
     //��û�д�������ݳ���
     uint64_t unprocessed_;
     /* 160-bit algorithm internal hashing state */
     uint32_t hash_[5];
 } sha1_ctx;
 
 //�ڲ�������SHA1�㷨�������ĵĳ�ʼ��
 static void zen_sha1_init(sha1_ctx *ctx)
 {
     ctx->length_ = 0;
     ctx->unprocessed_ = 0;
     // ��ʼ���㷨�ļ���������ħ����
     ctx->hash_[0] = 0x67452301;
     ctx->hash_[1] = 0xefcdab89;
     ctx->hash_[2] = 0x98badcfe;
     ctx->hash_[3] = 0x10325476;
     ctx->hash_[4] = 0xc3d2e1f0;
 }
 
 
 /*!
 @brief      �ڲ���������һ��64bit�ڴ�����ժҪ(�Ӵ�)����
 @param      hash  ��ż���hash����ĵ�����
 @param      block Ҫ����Ĵ�����ڴ��
 */
 static void zen_sha1_process_block(uint32_t hash[5],
                                    const uint32_t block[HASH_SHA1_BLOCK_SIZE / 4])
 {
     size_t        t;
     uint32_t      wblock[80];
     register uint32_t      a, b, c, d, e, temp;
 
     //SHA1�㷨������ڲ�����Ҫ���Ǵ�ͷ���ģ���Сͷ�Ļ���ת��
 #if HASH_BYTES_ORDER == HASH_LITTLE_ENDIAN
     swap_uint32_memcpy(wblock, block, HASH_SHA1_BLOCK_SIZE);
 #else
     ::memcpy(wblock, block, HASH_SHA1_BLOCK_SIZE);
 #endif
 
     //����
     for (t = 16; t < 80; t++)
     {
         wblock[t] = ROTL32(wblock[t - 3] ^ wblock[t - 8] ^ wblock[t - 14] ^ wblock[t - 16], 1);
     }
 
     a = hash[0];
     b = hash[1];
     c = hash[2];
     d = hash[3];
     e = hash[4];
 
     for (t = 0; t < 20; t++)
     {
         /* the following is faster than ((B & C) | ((~B) & D)) */
         temp =  ROTL32(a, 5) + (((c ^ d) & b) ^ d)
                 + e + wblock[t] + 0x5A827999;
         e = d;
         d = c;
         c = ROTL32(b, 30);
         b = a;
         a = temp;
     }
 
     for (t = 20; t < 40; t++)
     {
         temp = ROTL32(a, 5) + (b ^ c ^ d) + e + wblock[t] + 0x6ED9EBA1;
         e = d;
         d = c;
         c = ROTL32(b, 30);
         b = a;
         a = temp;
     }
 
     for (t = 40; t < 60; t++)
     {
         temp = ROTL32(a, 5) + ((b & c) | (b & d) | (c & d))
                + e + wblock[t] + 0x8F1BBCDC;
         e = d;
         d = c;
         c = ROTL32(b, 30);
         b = a;
         a = temp;
     }
 
     for (t = 60; t < 80; t++)
     {
         temp = ROTL32(a, 5) + (b ^ c ^ d) + e + wblock[t] + 0xCA62C1D6;
         e = d;
         d = c;
         c = ROTL32(b, 30);
         b = a;
         a = temp;
     }
 
     hash[0] += a;
     hash[1] += b;
     hash[2] += c;
     hash[3] += d;
     hash[4] += e;
 }
 
 
 /*!
 @brief      �ڲ��������������ݵ�ǰ�沿��(>64�ֽڵĲ���)��ÿ�����һ��64�ֽڵ�block�ͽ����Ӵմ���
 @param      ctx  �㷨�������ģ���¼�м����ݣ������
 @param      msg  Ҫ���м��������buffer
 @param      size ����
 */
 static void zen_sha1_update(sha1_ctx *ctx,
                             const unsigned char *buf, 
                             size_t size)
 {
     //Ϊ����zen_sha1_update���Զ�ν��룬���ȿ����ۼ�
     ctx->length_ += size;
 
     //ÿ������Ŀ鶼��64�ֽ�
     while (size >= HASH_SHA1_BLOCK_SIZE)
     {
         zen_sha1_process_block(ctx->hash_, reinterpret_cast<const uint32_t *>(buf));
         buf  += HASH_SHA1_BLOCK_SIZE;
         size -= HASH_SHA1_BLOCK_SIZE;
     }
 
     ctx->unprocessed_ = size;
 }
 
 
 /*!
 @brief      �ڲ��������������ݵ���󲿷֣����0x80,��0�����ӳ�����Ϣ
 @param      ctx    �㷨�������ģ���¼�м����ݣ������
 @param      msg    Ҫ���м��������buffer
 @param      result ���صĽ��
 */
 static void zen_sha1_final(sha1_ctx *ctx, 
                            const unsigned char *msg,
                            size_t size, 
                            unsigned char *result)
 {
 
     uint32_t message[HASH_SHA1_BLOCK_SIZE / 4];
 
     //����ʣ������ݣ�����Ҫƴ�����1��������������Ҫ����Ŀ飬ǰ����㷨��֤�ˣ����һ����϶�С��64���ֽ�
     if (ctx->unprocessed_)
     {
         memcpy(message, msg + size - ctx->unprocessed_, static_cast<size_t>( ctx->unprocessed_));
     }
 
     //�õ�0x80Ҫ����ڵ�λ�ã���uint32_t �����У���
     uint32_t index = ((uint32_t)ctx->length_ & 63) >> 2;
     uint32_t shift = ((uint32_t)ctx->length_ & 3) * 8;
 
     //���0x80��ȥ�����Ұ����µĿռ䲹��0
     message[index]   &= ~(0xFFFFFFFF << shift);
     message[index++] ^= 0x80 << shift;
 
     //������block���޷����������ĳ����޷����ɳ���64bit����ô�ȴ������block
     if (index > 14)
     {
         while (index < 16)
         {
             message[index++] = 0;
         }
 
         zen_sha1_process_block(ctx->hash_, message);
         index = 0;
     }
 
     //��0
     while (index < 14)
     {
         message[index++] = 0;
     }
 
     //���泤�ȣ�ע����bitλ�ĳ���,����������ҿ��������˰��죬
     uint64_t data_len = (ctx->length_) << 3;
 
     //ע��SHA1�㷨Ҫ���64bit�ĳ����Ǵ�ͷBIG-ENDIAN����Сͷ������Ҫ����ת��
 #if HASH_BYTES_ORDER == HASH_LITTLE_ENDIAN
     data_len = HASH_SWAP_UINT64(data_len);
 #endif
 
     message[14] = (uint32_t) (data_len & 0x00000000FFFFFFFF);
     message[15] = (uint32_t) ((data_len & 0xFFFFFFFF00000000ULL) >> 32);
 
     zen_sha1_process_block(ctx->hash_, message);
 
     //ע�����Ǵ�ͷ���ģ���Сͷ������Ҫ����ת��
 #if HASH_BYTES_ORDER == HASH_LITTLE_ENDIAN
     swap_uint32_memcpy(result, &ctx->hash_, SHA1_HASH_SIZE);
 #else
     memcpy(result, &ctx->hash_, SHA1_HASH_SIZE);
 #endif
 }
 
 
 
 //����һ���ڴ����ݵ�SHA1ֵ
 unsigned char* HashUtility::sha1(const unsigned char *msg,
                              size_t size,
                              unsigned char result[SHA1_HASH_SIZE])
 {
     assert(result != NULL);
 
     sha1_ctx ctx;
     zen_sha1_init(&ctx);
     zen_sha1_update(&ctx, msg, size);
     zen_sha1_final(&ctx, msg, size, result);
     return result;
 }
 
 std::string HashUtility::toString( unsigned char* pResult, size_t len )
 {
	 char a = 0;
	 char b = 0;
	 size_t bufferSize = len * 2 + 1;
	 char* pstrResult = new char[bufferSize];
	 memset( pstrResult, 0, bufferSize);
	 for( int idx = 0; idx < (int)len; ++idx )
	 {
		 a = pResult[idx] >> 4;
		 b = pResult[idx] & 0x0F;
		 sprintf_s( pstrResult + idx * 2, 3, "%X%X", a, b );
	 }

	 std::string strResult = pstrResult;
	 delete[] pstrResult;
	 return strResult;
 }

