#pragma once

 ///MD5�Ľ�����ݳ���
 static const size_t MD5_HASH_SIZE   = 16;
 ///SHA1�Ľ�����ݳ���
 static const size_t SHA1_HASH_SIZE  = 20;
 
 namespace HashUtility
 {
 
 
 /*!
 @brief      ��ĳ���ڴ���MD5��
 @return     unsigned char* ���صĵĽ����
 @param[in]  buf    ��MD5���ڴ�BUFFERָ��
 @param[in]  size   BUFFER����
 @param[out] result ���
 */
 unsigned char *md5(const unsigned char *buf,
                    size_t size,
                    unsigned char result[MD5_HASH_SIZE]);
 
 
 /*!
 @brief      ���ڴ��BUFFER��SHA1ֵ
 @return     unsigned char* ���صĵĽ��
 @param[in]  buf    ��SHA1���ڴ�BUFFERָ��
 @param[in]  size   BUFFER����
 @param[out] result ���
 */
 unsigned char *sha1(const unsigned char *buf,
                     size_t size,
                     unsigned char result[SHA1_HASH_SIZE]);

 std::string toString( unsigned char* pResult, size_t len );

 };

