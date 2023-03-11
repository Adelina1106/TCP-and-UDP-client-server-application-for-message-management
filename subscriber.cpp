#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include "utils.h"

void usage(char *file)
{
	fprintf(stderr, "Usage: %s server_address server_port\n", file);
	exit(0);
}

int main(int argc, char *argv[])
{
	int sockfd, n, ret;
	struct sockaddr_in serv_addr;
	char buffer[1600];

	if (argc < 4)
	{
		usage(argv[0]);
	}
	fd_set read_fds;
	fd_set tmp_fds;
	setvbuf(stdout, NULL, _IONBF, BUFSIZ);
	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	DIE(sockfd < 0, "socket");

	// completez serv_addr
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(atoi(argv[3]));
	ret = inet_aton(argv[2], &serv_addr.sin_addr);
	DIE(ret == 0, "inet_aton");

	ret = connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
	DIE(ret < 0, "connect");
	memset(buffer, 0, 1600);
	strcpy(buffer, argv[1]);

	// dezactivare algoritm Nagle
	int flag = 1;
	int res = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

	// trimit catre server mesajul care il anunta ca s-a conectat un nou client
	tcp_message message;
	strcpy(message.id_client, argv[1]);
	strcpy(message.message, argv[1]);
	strcpy(buffer, argv[1]);
	ret = send(sockfd, &message, sizeof(message), 0);
	DIE(ret < 0, "send");

	// setez file descriptorii pentru socketi
	int fdmax = sockfd;
	FD_SET(sockfd, &read_fds);
	FD_SET(0, &read_fds);

	while (1)
	{
		tmp_fds = read_fds;
		int rc = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
		DIE(rc < 0, "select");
		if (FD_ISSET(STDIN_FILENO, &tmp_fds))
		{
			tcp_message msg;
			memset(msg.message, 0, sizeof(msg.message));
			fgets(msg.message, 1600, stdin);
			strcpy(msg.id_client, argv[1]);
			// afisez in subscriber evenimentele de abonare ale acestuia
			if (strncmp(msg.message, "subscribe", 9) == 0)
			{
				printf("Subscribed to topic.\n");
			}
			if (strncmp(msg.message, "unsubscribe", 11) == 0)
			{
				printf("Unsubscribed to topic.\n");
			}
			if (strncmp(msg.message, "exit", 4) == 0)
			{
				ret = send(sockfd, &msg, sizeof(msg), 0);
				DIE(ret < 0, "send");
				close(sockfd);
				break;
			}

			// trimit mesaj la server cu comanda primita in campul message
			// al structurii pentru tcp
			n = send(sockfd, &msg, sizeof(tcp_message), 0);
			DIE(n < 0, "send");
		}
		else if (FD_ISSET(sockfd, &tmp_fds))
		{
			tcp_message msg;
			memset(buffer, 0, 1600);
			int dim, dimTotal, dimBuf;
			int i = 0;

			// verific daca s-a primit dimensiunea mesajului
			// conform mesajului trimis din server, stiu ca s-a primit
			// cand se primeste si delimitatorul END
			while (recv(sockfd, &buffer[i], 1, 0) > 0)
			{
				if ('D' == buffer[i] && 'N' == buffer[i - 1] && 'E' == buffer[i - 2])
					break;
				i++;
			}
			if (recv == 0)
			{
				close(sockfd);
				return 0;
			}

			// extrag dimensiunea mesajului ce trebuie afisat
			i = i - 2;
			buffer[i] = '\0';
			dim = atoi(buffer);

			int dim_initial = dim;
			int total_dim = 0;
			int ret = 1;
			int done = 0;

			// request pe socket de dimensiunea primita precedent pana cand
			// se primeste tot mesajul
			// verific ca s-a primit tot mesajul retinand daca dimensiunea
			// mesajului primit pana la un moment dat este egala cu dimensiunea
			// primita anterior(retinuta in dim)
			// s-a primit intreg mesajul cand acestea sunt egale
			while (ret > 0 && total_dim != dim_initial)
			{
				memset(buffer, 0, 1600);
				ret = recv(sockfd, buffer, dim, 0);
				DIE(ret < 0, "recv");

				// daca s-a primit mesajul exit, se inchide subscriber-ul
				if (strncmp(buffer, "exit", 4) == 0)
				{
					done = 1;
					break;
				}
				total_dim = total_dim + ret;
				// printez mesajul primit
				printf("%s", buffer);
			}
			if (done == 1)
				break;
		}
	}

	// inchide socket-ul
	close(sockfd);
	return 0;
}
