#ifndef LIBAGENT_SERVER_H
#define LIBAGENT_SERVER_H
#define PORT  (8888)

int init_srv(int port);
void *AgentServer(void *param);
#endif  //end of LIBAGENT_SERVER_H
