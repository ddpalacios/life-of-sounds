#include <stdio.h>
#include <uuid/uuid.h>
#include <ctype.h>
#include <string.h>
#include <openssl/sha.h>
#include <openssl/ssl.h>
#include <openssl/evp.h>
#include <time.h>
#include "SQL.h"


void delete_websocket_session(char* userid, char* sessionid){
	MYSQL* conn = connect_to_sql("testUser",  "testpwd","localhost", "Users");
	char sql[255];
	snprintf(sql, sizeof(sql),"DELETE FROM websocket_connection WHERE userid = '%s' AND sessionid = '%s'",
			userid,
			sessionid);
	//printf("query: %s\n", sql);
	query(conn, sql);
	close_sql_connection(conn);

}

void insert_websocket_session(char* userid, char* sessionid){
	MYSQL* conn = connect_to_sql("testUser",  "testpwd","localhost", "Users");
	char sql[255];
	time_t current_time = time(NULL);
	static char guid_str[37];
	uuid_t guid;
	uuid_unparse(guid, guid_str);
	static char date_string[20];
	strftime(date_string, 20, "%Y-%m-%d", localtime(&current_time));
	snprintf(sql, sizeof(sql),
			"INSERT INTO websocket_connection VALUES ('%s', '%s', '%s', '%s')",
			userid,
			sessionid,
			date_string,
			guid_str);
	query(conn, sql);
	close_sql_connection(conn);
}

char* generate_websocket_accptKey(char* websocket_sec_key ){
	char websocket_key[32];
	char* magic_key =  "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
	char combinedKey[200];
	snprintf(combinedKey, sizeof(combinedKey), "%s%s", websocket_sec_key, magic_key);
	unsigned char hash[SHA_DIGEST_LENGTH];
	SHA1((const unsigned char *) combinedKey, strlen(combinedKey), hash);
	static char base64Hash[SHA_DIGEST_LENGTH * 2];
	int base64Length = EVP_EncodeBlock((unsigned char *)base64Hash, hash, SHA_DIGEST_LENGTH);
	return base64Hash;


}
int is_websocket_buffer(unsigned char* buf){
	int finVal = buf[0] & 0x80;
	int opcode = buf[0] & 0x0F;
	int mask = buf[1] & 0x80;
	int payloadlength = buf[1] & 0x7F;
	if (opcode == 0 && finVal == 0 && mask==0){
		  return 0;
	  }
	if (opcode == 0 && finVal == 0 && mask == 128){
		  return 1;
	  }
	if (opcode >=1 && mask == 128){
		return 1;
	  
	}else{
		return 0;
	  }
}


void send_websocket_buffer(SSL* cSSL, char* buf){
	unsigned char frame[2 + strlen(buf)];
	frame[0] = 0x81; 
	frame[1] = strlen(buf);
	for (int i =0; i<strlen(buf); i++){
		frame[2+i] = buf[i];
	
	}
	SSL_write(cSSL, buf, 2+strlen(buf));



}

int  decode_websocket_buffer(char* buf, char message[] ){

    // Byte 1
    int finVal = buf[0] & 0x80; // Bit 0
    int opcode = buf[0] & 0x0F; // Bits 4-7

    // Bytes 2 - 10 Payload length
    int mask = buf[1] & 0x80; // Bit 8 Must expect this to be 1

    //printf("finVal %d, opcode: %d, mask: %d\n",finVal,opcode,mask);
    if (mask){
	    int payloadlength = buf[1] & 0x7F;
	    //printf("Initial Payload Length int: %d\n",buf[1]);
	    //printf("Initial Payload Length : %d\n",254 & 0x7F);
	    unsigned char maskingKey[4];
	    if (payloadlength < 126){
		    int offset = 2;
		    for (int i =0; i<4; i++){
			maskingKey[i] = buf[2+i];
			offset = 2 +i;
		    }
		    ++offset;
		    unsigned char payload[payloadlength+1];
		    for (int i =0; i<payloadlength; i++){
			    payload[i] = buf[offset+i];
		    }
		    for (int i=0; i<payloadlength; i++){
			int message_val = payload[i] ^ maskingKey[i%4];
			message[i] = message_val;
		    }
		    message[payloadlength] = '\0';
	    }

	    if (payloadlength == 126){
		    unsigned int p1 = buf[2] & 0xFF;
		    unsigned int p2 = buf[3] & 0xFF;
		    payloadlength = (p1 << 8) | p2;
		    //printf("Payload Length Extracted: %d\n",payloadlength);
		    int offset = 4;
		    for (int i =0; i<4; i++){
			maskingKey[i] = buf[4+i];
			offset = 4 +i;
		    }
		    ++offset;
		    unsigned char payload[payloadlength+1];
		    for (int i =0; i<payloadlength; i++){
			    payload[i] = buf[offset+i];
		    }
		    for (int i=0; i<payloadlength; i++){
			int message_val = payload[i] ^ maskingKey[i%4];
			message[i] = message_val;
		    }
		    message[payloadlength] = '\0';
	    }
	    //printf("True payload Length : %d\n",payloadlength);
	    return payloadlength;
    }
}

