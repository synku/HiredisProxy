#include "../redismodule.h"
#include "../rmutil/util.h"
#include "../rmutil/strings.h"
#include "../rmutil/test_util.h"

#include "../hiredis/hiredis.h"

redisContext *c;

/*
	Output must be freed
*/
char *GetPassword(RedisModuleCtx *ctx)
{
	size_t len;
	char *result = NULL;

	RedisModuleString *param1 = RedisModule_CreateString(ctx, "GET", 3);
	RedisModuleString *param2 = RedisModule_CreateString(ctx, "REQUIREPASS", 11);
	RedisModuleCallReply *reply = RedisModule_Call(ctx, "CONFIG", "ss", param1, param2);

	if(reply != NULL) {
		RedisModuleCallReply *subreply;
		subreply = RedisModule_CallReplyArrayElement(reply,1);
		const char *ptr = RedisModule_CallReplyStringPtr(subreply,&len);
		if(ptr !=NULL) {
			result = strndup(ptr, len);
		}
	}

	return result;
}

/*
	Output must be freed
*/
char *GetSubreplyString(RedisModuleCallReply *roleReply, int idx)
{
	size_t len;

	RedisModuleCallReply *subreply;
	subreply = RedisModule_CallReplyArrayElement(roleReply,idx);
	const char *ptr = RedisModule_CallReplyStringPtr(subreply,&len);
	if(ptr !=NULL) {
		return strndup(ptr, len);
	}

	return NULL;
}

int GetSubreplyInt(RedisModuleCallReply *roleReply, int idx)
{
	RedisModuleCallReply *subreply;
	subreply = RedisModule_CallReplyArrayElement(roleReply,idx);
	return (int)RedisModule_CallReplyInteger(subreply);
}


int HiredisConnect(RedisModuleCtx *ctx)
{
	// Hi-redis init
	struct timeval timeout = { 0, 500000 }; // 0.5 seconds

	// Calling ROLE and parsing reply
	RedisModuleCallReply *reply = RedisModule_Call(ctx, "ROLE", "");
	RMUTIL_ASSERT_NOERROR(reply);

	unsigned int reply_len = (unsigned int)RedisModule_CallReplyLength(reply);
	if(reply_len != 5) {
		return 0;	// Not a normal response size for ROLE when slave, see http://redis.io/commands/role
	}

	char *hostname = GetSubreplyString(reply, 1);
	int port = GetSubreplyInt(reply, 2);

	// Calling CONFIG GET and grabbing reply as string
	char *password = GetPassword(ctx);

//	printf("host = [%s:%d], password = [%s]\n", hostname, port, password);
	c = redisConnectWithTimeout(hostname, port, timeout);

	free(hostname);
	free(password);

	if (c == NULL || c->err) {
		if (c) {
		  redisFree(c);
		  printf("Connection error: %s\n", c->errstr);
		} else {
		  printf("Connection error: can't allocate redis context\n");
		}
		return 0;
	}

	redisSetTimeout(c, timeout);

	// TODO: Unclear if this is needed
//	redisEnableKeepAlive(c);
	
	return 1;
}

/* HIREDIS.PROXY
*/
int HiredisProxy(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {

	// Hi-redis init
	redisReply *reply;

	// init auto memory for created strings
    RedisModule_AutoMemory(ctx);

	if(c == NULL && HiredisConnect(ctx) == 0) {
		RedisModule_ReplyWithError(ctx, "Unable to connect to master");
		return REDISMODULE_ERR;
	}

	// First attempt on existing connection, second attempt retrying to connect
	int attempts = 2;

	while(1) {
		switch(argc) {
			case 2:
				reply = redisCommand(c, RedisModule_StringPtrLen(argv[1], NULL));
				break;
			case 3:
				reply = redisCommand(c, "%s %s", 
					RedisModule_StringPtrLen(argv[1], NULL), 
					RedisModule_StringPtrLen(argv[2], NULL)
				);
				break;
			case 4:
				reply = redisCommand(c, "%s %s %s", 
					RedisModule_StringPtrLen(argv[1], NULL), 
					RedisModule_StringPtrLen(argv[2], NULL),
					RedisModule_StringPtrLen(argv[3], NULL)
				);
				break;
			case 5:
				reply = redisCommand(c, "%s %s %s %s", 
					RedisModule_StringPtrLen(argv[1], NULL), 
					RedisModule_StringPtrLen(argv[2], NULL),
					RedisModule_StringPtrLen(argv[3], NULL),
					RedisModule_StringPtrLen(argv[3], NULL)
				);
				break;
			default:
				return RedisModule_WrongArity(ctx);
		}

		// success or no more attempts?
		attempts--;
		if(reply != NULL || !attempts) {
			break;
		}

		// reconnect?
		if(reply == NULL) {
			if(HiredisConnect(ctx) == 0) {
				printf("Failed to reconnect.\n");
			}
			else {
				printf("Reconnected.\n");
			}
		}
	}

	if(reply == NULL) {
		RedisModule_ReplyWithError(ctx, "Connection with server was lost");
		return REDISMODULE_ERR;
	}
	else {
		if(reply->str != NULL) {
			RedisModule_ReplyWithSimpleString(ctx, reply->str);
		}
		else {
			RedisModule_ReplyWithNull(ctx);
		}
		freeReplyObject(reply);
		return REDISMODULE_OK;
	}
}

int RedisModule_OnLoad(RedisModuleCtx *ctx) {

	// Register the module itself
	if (RedisModule_Init(ctx, "hiredis", 1, REDISMODULE_APIVER_1) ==
	    REDISMODULE_ERR) {
	  return REDISMODULE_ERR;
	}
	
	// register hiredis.proxy - the default registration syntax
	if (RedisModule_CreateCommand(ctx, "hiredis.proxy", HiredisProxy, "readonly",
	                              1, 1, 1) == REDISMODULE_ERR) {

		return REDISMODULE_ERR;
	}

	return REDISMODULE_OK;
}