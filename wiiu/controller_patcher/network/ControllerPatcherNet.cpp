#include "ControllerPatcherNet.hpp"
#include "wiiu/os.h"
#include "sys/socket.h"

s32 ControllerPatcherNet::recvwait(s32 sock, void *buffer, s32 len) {
	s32 ret;
	while (len > 0) {
		ret = recv(sock, buffer, len, 0);
		if(ret < 0) return ret;
		len -= ret;
		buffer =  (void *)(((char *) buffer) + ret);
	}
	return 0;
}

u8 ControllerPatcherNet::recvbyte(s32 sock) {
	unsigned char buffer[1];
	s32 ret;

	ret = recvwait(sock, buffer, 1);
	if (ret < 0) return ret;
	return buffer[0];
}

s32 ControllerPatcherNet::checkbyte(s32 sock) {
	unsigned char buffer[1];
	s32 ret;

	ret = recv(sock, buffer, 1, MSG_DONTWAIT);
	if (ret < 0) return ret;
	if (ret == 0) return -1;
	return buffer[0];
}

s32 ControllerPatcherNet::sendwait(s32 sock, const void *buffer, s32 len) {
	s32 ret;
	while (len > 0) {
		ret = send(sock, buffer, len, 0);
		if(ret < 0) return ret;
		len -= ret;
		buffer =  (void *)(((char *) buffer) + ret);
	}
	return 0;
}

s32 ControllerPatcherNet::sendbyte(s32 sock, unsigned char byte) {
	unsigned char buffer[1];
	buffer[0] = byte;
	return sendwait(sock, buffer, 1);
}
