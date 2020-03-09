#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <plugin_ext_def.h>


#define HTTP_MESSAGE "POST /monitor/data HTTP/1.1\r\nUser-Agent: Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/80.0.3987.122 Safari/537.36\r\nConnect:close\r\nContent-Type:application/x-www-form-urlencoded\r\nContent-Length:%d\r\n\r\n"

int http_req(char ip[16], short port, const char* data, unsigned data_len)
{

    int fd;
    struct sockaddr_in serv_addr;
    char* pbuf;
    unsigned len;

    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Create Socket");
    } else {
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_addr.s_addr = inet_addr(ip);
        serv_addr.sin_port = htons(port);
        if (connect(fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr))<0) {
            perror("Bind socket");
        } else {
            len = strlen(HTTP_MESSAGE)+data_len + 128;
            pbuf = (char*)malloc(len);
            if (pbuf != NULL) {
                sprintf(pbuf, HTTP_MESSAGE, data_len);
                strncat(pbuf, data, data_len);
                //memcpy(pbuf+strlen(HTTP_MESSAGE), data, data_len);
                send(fd, pbuf, len, 0);
                free(pbuf);
            }
            close(fd);
        }
    }
            
    return 0;
}

int send_data(const item_t *data)
{
    char varbuf[128] = {0x0};
    char *buf = (char*)malloc(data->element_num*2048);
    if (buf != NULL) {
        memset(buf, 0x0, data->element_num*2048);
        sprintf(buf,"cpu={");
        for (int i=0; i<data->element_num; i++) {
            strcat(buf, "\"");
            strcat(buf, data->data[i].name);
            strcat(buf, "\":");
            sprintf(varbuf, "%.1f", data->data[i].val.f);
            strcat(buf, varbuf);
            strcat(buf, ",");
        }
        strcat(buf, "}");
        //printf("[ OUT ] %s\n",buf);
        http_req("192.168.10.99", 5000, buf, strlen(buf));
        free(buf);
    }
}

//int main(int argc, char* argv[])
//{
//    item_t item;
//    item.element_num = 3;
//    item.data = (mate_t*)malloc(sizeof(mate_t)*3);
//    if (item.data != NULL) {
//        for (int j = 0; j < atoi(argv[1]); j++) {
//            for (int i = 0; i<item.element_num; i++) {
//                sprintf(item.data[i].name, "name_%d", i);
//                item.data[i].val.f = 2.5 * (i+1+j);
//            }
//            send_data(&item);
//            sleep(1);
//        }
//        free(item.data);
//    }
//    
//    return 0;
//}

