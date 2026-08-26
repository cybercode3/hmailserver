// Copyright (c) 2010 Martin Knafve / hMailServer.com.  
// http://www.hmailserver.com

#include <stdafx.h>

#include "../PasswordGenerator.h"
#include "../../Mime/MimeCode.h"

#include <openssl/evp.h>

#include "HashCreator.h"

#ifdef _DEBUG
#define DEBUG_NEW new(_NORMAL_BLOCK, __FILE__, __LINE__)
#define new DEBUG_NEW
#endif

namespace HM
{
   HashCreator::HashCreator(HashCreator::HashType hashType) :
      hash_type_(hashType)
   {

   }

   AnsiString HashCreator::GenerateHash(const AnsiString &inputString, const AnsiString &salt)
   {
      AnsiString saltString = salt;
      if (saltString.GetLength() == 0 && hash_type_ == SHA256)
      {     
         AnsiString randomString = PasswordGenerator::Generate();
         saltString = GetHash_(randomString, hex);
         saltString = saltString.Mid(0, SALT_LENGTH);
      }

      AnsiString value = saltString + GetHash_(saltString + inputString, hex);
      return value;
   }

   AnsiString 
   HashCreator::GenerateHashNoSalt(const AnsiString &inputString, RequestedEncoding encoding)
   {
      return GetHash_(inputString, encoding);
   }

   AnsiString 
   HashCreator::GenerateHashNoSalt(unsigned char *input, int inputLength, RequestedEncoding encoding)
   {
      return GetHash_Raw(input, inputLength, encoding);
   }


   bool 
   HashCreator::ValidateHash(const AnsiString &password, const AnsiString &originalHash, bool useSalt)
   {
      if (useSalt)
      {
         AnsiString salt = GetSalt_(originalHash);
         AnsiString result = GenerateHash(password, salt);

         if (result == originalHash)
            return true;
         else
            return false;
      }
      else
      {
         AnsiString result = GetHash_(password, hex);

         if (result == originalHash)
            return true;
         else
            return false;
      }


   }

   AnsiString HashCreator::GetSalt_(const AnsiString &inputString)
   {
      AnsiString result = inputString.Mid(0,SALT_LENGTH);
      return result;
   }

   AnsiString HashCreator::GetHash_(const AnsiString &sInputString, HashCreator::RequestedEncoding encoding)
   {
      AnsiString temp = sInputString;
      return GetHash_Raw((unsigned char*) temp.GetBuffer(), temp.GetLength(), encoding);
   }

   AnsiString HashCreator::GetHash_Raw(const unsigned char *input, int inputLength, HashCreator::RequestedEncoding encoding)
   {
      const EVP_MD *message_digest = nullptr;

      switch (hash_type_)
      {
      case SHA1:
         message_digest = EVP_sha1();
         break;
      case SHA256:
         message_digest = EVP_sha256();
         break;
      case MD5:
         message_digest = EVP_md5();
         break;
      }

      if (!message_digest)
         return AnsiString();

      unsigned char results[EVP_MAX_MD_SIZE];
      unsigned int digest_length = 0;

      if (EVP_Digest(input, static_cast<size_t>(inputLength), results, &digest_length, message_digest, nullptr) != 1)
         return AnsiString();

      HM::AnsiString retVal;
      if (encoding == hex)
      {
         char buffer[3];
         buffer[2] = '\0';

         for (unsigned int i = 0; i < digest_length; i++)
         {
            sprintf_s(buffer, 3, "%02x", results[i]);

            retVal += buffer;
         }

      }
      else if (encoding == base64)
      {
         MimeCodeBase64 encoder;
         encoder.SetInput((const char*) results, static_cast<int>(digest_length), true);

         AnsiString sEncodedValue;
         encoder.GetOutput(sEncodedValue);

         retVal = sEncodedValue;
         retVal = retVal.Mid(0, retVal.GetLength()-2);
      }

      return retVal;
   }

   void
   HashCreatorTester::Test()
   {
      // Run basic test.
      HashCreator hasher(HashCreator::SHA256);
      AnsiString result = hasher.GenerateHash("The quick brown fox jumps over the lazy dog", "");

      if (!hasher.ValidateHash("The quick brown fox jumps over the lazy dog", result, true))
         throw 0;

      // Check that same password hashed twice yealds separate hashes.
      AnsiString test1 = hasher.GenerateHash("The quick brown fox jumps over the lazy dog", "");
      AnsiString test2 = hasher.GenerateHash("The quick brown fox jumps over the lazy dog", "");
      if (test1 == test2)
         throw 0;

      for (int i = 0; i < 250; i++)
      {
         HashCreator memoryTester(HashCreator::SHA256);

         String temp;
         temp.Format(_T("%d"), i);
         AnsiString hashableString = temp;

         hasher.GenerateHash(hashableString, "test");
      }
   }
}