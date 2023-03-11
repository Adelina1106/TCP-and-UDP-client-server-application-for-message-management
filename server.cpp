#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <cstring>
#include <netinet/tcp.h>
#include "utils.h"
using namespace std;

#define BUFLEN 1600
#define MESSAGE_LEN 1550
#define TOPIC_END 50
#define CLIENTS 2000

// structura folosita pentru mesajele
// primite de la clientii UDP
typedef struct
{
	char topic[51];
	char tip_date;
	char payload[1501];
} udp_message;

// structura folosita pentru un client
typedef struct
{
	char id_client[10];
	int sockfd;
} client;

void usage(char *file)
{
	fprintf(stderr, "Usage: %s server_port\n", file);
	exit(0);
}

int main(int argc, char *argv[])
{
	// Primul vector retine topic-urile ce au abonati, iar cel de-al doilea abonatii
	//(pe pozitia i se afla abonatii pentru topic-ul de pe pozitia i din topics)
	vector<string> topics;
	vector<vector<client>> subscribers_of_topic;

	// clients_id - contine id-urile clientilor abonati la topic-uri cu SF = 1
	// topics_SF - pe pozitia i contine topic-urile la care clientul de pe
	// pozitia i din clients_id este abonat cu SF = 1
	// to_receive - pe pozitia i contine mesajele pe care clientul de pe
	// pozitia i din clients_id trebuie sa le primeasca daca s-ar deconecta
	vector<string> clients_id;
	vector<vector<string>> topics_SF;
	vector<vector<string>> to_receive;

	// vectorul ce tine minte clientii conectati
	vector<client> connected;

	setvbuf(stdout, NULL, _IONBF, BUFSIZ);
	int sockfd_udp, sockfd_tcp, newsockfd, portno;
	char buffer[BUFLEN];
	struct sockaddr_in serv_addr, cli_addr;
	int n, i, ret;
	socklen_t clilen = sizeof(struct sockaddr_in);

	fd_set read_fds; // multimea de citire folosita in select()
	fd_set tmp_fds;	 // multime folosita temporar
	int fdmax;		 // valoare maxima fd din multimea read_fds

	if (argc < 2)
	{
		usage(argv[0]);
	}

	// se goleste multimea de descriptori de citire (read_fds) si multimea temporara (tmp_fds)
	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

	portno = atoi(argv[1]);
	DIE(portno == 0, "atoi");

	memset((char *)&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(portno);
	serv_addr.sin_addr.s_addr = INADDR_ANY;

	// socket TCP
	sockfd_tcp = socket(AF_INET, SOCK_STREAM, 0);
	DIE(sockfd_tcp < 0, "socket");
	int flag = 1;
	int res = setsockopt(sockfd_tcp, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
	ret = bind(sockfd_tcp, (struct sockaddr *)&serv_addr, sizeof(struct sockaddr));
	DIE(ret < 0, "bind");
	ret = listen(sockfd_tcp, CLIENTS);
	DIE(ret < 0, "listen");

	// socket UDP
	sockfd_udp = socket(AF_INET, SOCK_DGRAM, 0);
	DIE(sockfd_udp < 0, "socket");
	ret = bind(sockfd_udp, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
	DIE(ret < 0, "bind");
	FD_SET(0, &read_fds);
	FD_SET(sockfd_udp, &read_fds);
	FD_SET(sockfd_tcp, &read_fds);

	if (sockfd_udp > sockfd_tcp)
		fdmax = sockfd_udp;
	else
		fdmax = sockfd_tcp;

	while (1)
	{
		tmp_fds = read_fds;
		ret = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
		DIE(ret < 0, "select");
		for (i = 0; i <= fdmax; i++)
		{
			if (FD_ISSET(i, &tmp_fds))
			{
				if (i == sockfd_tcp)
				{
					// primire Tcp clienti noi
					clilen = sizeof(cli_addr);
					newsockfd = accept(sockfd_tcp, (struct sockaddr *)&cli_addr, (socklen_t *)&clilen);
					DIE(newsockfd < 0, "accept");

					memset(buffer, 0, 1600);
					tcp_message msg;
					ret = recv(newsockfd, &msg, sizeof(msg), 0);
					DIE(ret < 0, "recv");

					// verific daca noul client este deja conectat(se afla in vectorul connected)
					int found = 0;
					for (int i = 0; i < connected.size(); i++)
						if (strcmp((connected.at(i)).id_client, msg.id_client) == 0)
						{
							printf("Client %s already connected.\n", msg.id_client);
							tcp_message message;
							strcpy(message.message, "exit");
							sprintf(buffer, "%dEND", (int)strlen(message.message));
							ret = send(newsockfd, buffer, strlen(buffer), 0);
							DIE(ret < 0, "send");
							ret = send(newsockfd, &message, strlen(message.message) + 1, 0);
							DIE(ret < 0, "send");
							found = 1;
							break;
						}

					// clientul nu este conectat
					if (found == 0)
					{
						// setez noul socket
						FD_SET(newsockfd, &read_fds);
						if (newsockfd > fdmax)
						{
							fdmax = newsockfd;
						}
						// dezactivez algoritmul Nagle
						int res = setsockopt(newsockfd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
						// construiesc structura pentru noul client
						client subscriber;
						strcpy(subscriber.id_client, msg.id_client);
						subscriber.sockfd = newsockfd;

						// adaug noul client la vectorul celor conectati
						connected.push_back(subscriber);

						// verific daca noul client s-a reconectat si are de primit mesaje
						//(era abonat cu SF = 1 anterior)
							for (int i = 0; i < clients_id.size(); i++)
								if (msg.id_client == clients_id.at(i))
								{
									for (int j = 0; j < to_receive.at(i).size(); j++)
									{
										tcp_message message;
										strcpy(message.message, to_receive.at(i).at(j).c_str());
										char buffer_aux[BUFLEN];
										memset(buffer_aux, 0, BUFLEN);
										sprintf(buffer_aux, "%dEND", (int)strlen(message.message));
										ret = send(newsockfd, buffer_aux, strlen(buffer_aux), 0);
										DIE(ret < 0, "send");
										ret = send(newsockfd, &message, strlen(message.message), 0);
										DIE(ret < 0, "send");
									}
									// elimin mesajele care s-au trimis din lista de mesaje ce ar
									// trebui trimise pentru client
									to_receive.at(i).clear();
								}

						// Afisez in server ca s-a conectat un nou client
						printf("New client %s connected from %s:%d.\n",
							   msg.id_client, inet_ntoa(cli_addr.sin_addr), ntohs(serv_addr.sin_port));
					}
				}
				else if (i == sockfd_udp)
				{

					// s-au primit date pe socket-ul UDP
					memset(buffer, 0, 1600);
					ret = recvfrom(i, buffer, 1600, 0, (struct sockaddr *)&cli_addr, &clilen);
					DIE(ret < 0, "recv");

					// construiesc structura pentru clientii UDP
					udp_message msg;
					memset(msg.payload, 0, 1600);
					// mesaje de tip INT
					if (buffer[TOPIC_END] == 0)
					{
						uint32_t num;
						memcpy(&num, &(buffer[TOPIC_END + 2]), sizeof(uint32_t));
						num = ntohl(num);
						if (buffer[TOPIC_END + 1] == 1)
							sprintf(msg.payload, "%d", (-1) * num);
						else
							sprintf(msg.payload, "%d", num);

						msg.tip_date = 0;
						memcpy(msg.topic, &buffer, 50);
						msg.topic[50] = '\0';
					}
					// mesaje de tip SHORT_REAL
					else if (buffer[TOPIC_END] == 1)
					{
						uint16_t num;
						memcpy(&num, &(buffer[TOPIC_END + 1]), sizeof(uint16_t));
						num = ntohs(num);
						double num_double = 1.0 * num / 100;
						sprintf(msg.payload, "%.2f", num_double);
						msg.tip_date = 1;
						memcpy(msg.topic, &buffer, 50);
						msg.topic[TOPIC_END] = '\0';
					}
					// mesaje de tip FLOAT
					else if (buffer[TOPIC_END] == 2)
					{

						double num = ntohl(*(uint32_t *)&buffer[TOPIC_END + 2]);
						uint8_t power = (*(uint8_t *)(&buffer[TOPIC_END + 2] + sizeof(uint32_t)));

						for (int i = 0; i < (int)power; i++)
							num = 1.0 * num / 10;

						if (buffer[TOPIC_END + 1] == 1)
							sprintf(msg.payload, "-%f", 1.0 * num);
						else
							sprintf(msg.payload, "%f", 1.0 * num);
						msg.tip_date = 2;
						memcpy(msg.topic, &buffer, TOPIC_END);
						msg.topic[TOPIC_END] = '\0';
					}
					// mesaje de tip STRING
					else if (buffer[TOPIC_END] == 3)
					{
						memcpy(msg.topic, &buffer, TOPIC_END);
						memcpy(msg.payload, &buffer[TOPIC_END + 1], 1500);
						msg.tip_date = 3;
						msg.topic[TOPIC_END] = '\0';
					}

					// formez mesajul ce va fi trimis clientilor pentru a-l afisa
					memset(buffer, 0, 1600);
					if (msg.tip_date == 0)
						sprintf(buffer, "%s:%d - %s - INT - %s\n", inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port), msg.topic, msg.payload);
					else if (msg.tip_date == 1)
						sprintf(buffer, "%s:%d - %s - SHORT_REAL - %s\n", inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port), msg.topic, msg.payload);
					else if (msg.tip_date == 2)
						sprintf(buffer, "%s:%d - %s - FLOAT - %s\n", inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port), msg.topic, msg.payload);
					else if (msg.tip_date == 3)
						sprintf(buffer, "%s:%d - %s - STRING - %s\n", inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port), msg.topic, msg.payload);

					char buffer_aux[BUFLEN];
					memset(buffer_aux, 0, BUFLEN);
					int len = strlen(buffer);

					tcp_message msg_tcp;
					strcpy(msg_tcp.message, buffer);

					// extrag dimensiunea mesajului ce va trebui trimis catre subscriber
					// si ii adaug delimitatorul ales(END)
					sprintf(buffer_aux, "%dEND", len);

					// trimit noul mesaj UDP catre toti clientii care sunt abonati
					// la topic-ul mesajului si care sunt conectati
					for (int i = 0; i < topics.size(); i++)
						if (msg.topic == topics.at(i))
							for (int j = 0; j < subscribers_of_topic.at(i).size(); j++)
								for (int k = 0; k < connected.size(); k++)
									if (subscribers_of_topic.at(i).at(j).sockfd == connected.at(k).sockfd)
									{
										// trimit dimensiunea mesajului si delimitatorul
										ret = send(subscribers_of_topic.at(i).at(j).sockfd, buffer_aux, strlen(buffer_aux), 0);
										DIE(ret < 0, "send");
										// trimit mesajul propriu zis(structura pentru tcp ce il contine)
										ret = send(subscribers_of_topic.at(i).at(j).sockfd, &msg_tcp, len, 0);
										DIE(ret < 0, "send");
									}

					// verific daca pentru topic-ul mesajului primit exista
					// abonati cu SF = 1 care nu sunt conectati
					for (int i = 0; i < topics_SF.size(); i++)
						for (int j = 0; j < topics_SF.at(i).size(); j++)
							if (msg.topic == topics_SF.at(i).at(j))
							{
								int found = 0;
								for (int k = 0; k < connected.size(); k++)
									if (connected.at(k).id_client == clients_id.at(i))
										found = 1;

								// daca exista abonati cu SF = 1 neconectati,
								// adaug mesajul in vectorul de mesaje pe care
								// acestia le vor primi cand se recorecteaza
								if (found == 0)
								{
									to_receive.at(i).push_back(buffer);
								}
							}
				}
				else if (i == 0)
				{
					memset(buffer, 0, 1600);
					fgets(buffer, 1599, stdin);

					// daca se primeste mesaj de la tastatura, se accepta doar exit
					// in acest caz, se trimit mesaje cu exit catre fiecare client
					if (strncmp(buffer, "exit\n", 5) == 0)
					{

						tcp_message msg;
						sprintf(buffer, "%dENDexit\n", (int)strlen("exit\n"));
						for (int i = 1; i <= fdmax; i++)
							if (FD_ISSET(i, &read_fds))
								// trimit mesaj de exit catre toti clientii
								if (i != sockfd_udp && i != sockfd_tcp)
								{
									tcp_message message;
									strcpy(message.message, "exit");
									sprintf(buffer, "%dEND", (int)strlen(message.message));
									ret = send(i, buffer, strlen(buffer), 0);
									DIE(ret < 0, "send");
									ret = send(i, &message, strlen(message.message) + 1, 0);
									DIE(ret < 0, "send");
								}
						close(sockfd_tcp);
						close(sockfd_udp);
						return 0;
					}
					else
						continue;
				}
				// se primesc mesaje pe socketii de client
				else
				{
					tcp_message msg_tcp;
					n = recv(i, &msg_tcp, sizeof(tcp_message), 0);
					DIE(n < 0, "recv");
					char topic[50];
					int SF;
					char command[11];
					if (strncmp(msg_tcp.message, "subscribe", 9) == 0)
					{
						// extrag tipul de mesaj(abonare/dezabonare/exit),
						// topic-ul si valoarea pentru SF
						sscanf(msg_tcp.message, "%s %s %d", command, topic, &SF);

						// daca clientul se aboneaza cu SF = 1, verific daca acesta
						// exista deja in vectorul cu clienti care sunt abonati cu Sf = 1 la
						// vreun topic
						if (SF == 1)
						{
							int found = 0;

							int j;
							for (j = 0; j < clients_id.size(); j++)
								if (clients_id.at(j) == msg_tcp.id_client)
								{
									found = 1;
									break;
								}
							// daca nu se afla, adaug un vector cu topic-ul in vectorul ce
							// retine topic-urile la care un client e abonat cu SF = 1(pe aceeasi
							// pozitie pe care o are clientul in lista de clienti abonati cu SF = 1)
							if (found == 0)
							{
								clients_id.push_back(msg_tcp.id_client);
								vector<string> aux;
								vector<string> aux1;
								aux.push_back(topic);
								topics_SF.push_back(aux);
								to_receive.push_back(aux1);
							}
							// daca se afla, adaug topic-ul in vectorul deja creat de la pozitia
							// respectiva
							else
							{
								topics_SF.at(j).push_back(topic);
							}
						}

						// pentru orice SF (0/1), verific daca topic-ul se afla deja in
						// lista de topic-uri cu abonati
						int found = 0, j;
						for (j = 0; j < topics.size(); j++)
							if (topics.at(j) == topic)
							{
								found = 1;
								break;
							}

						// daca se afla, adaug noul client in lista pentru clientii abonati
						// la un topic
						if (found == 1)
						{
							client subscriber;
							strcpy(subscriber.id_client, msg_tcp.id_client);
							subscriber.sockfd = i;
							(subscribers_of_topic.at(j)).push_back(subscriber);
						}
						// daca nu se afla, adaug topic-ul si clientul
						else
						{
							client subscriber;
							strcpy(subscriber.id_client, msg_tcp.id_client);
							subscriber.sockfd = i;
							topics.push_back(topic);

							vector<client> aux;
							aux.push_back(subscriber);
							subscribers_of_topic.push_back(aux);
						}
					}
					else if (strncmp(msg_tcp.message, "unsubscribe", 11) == 0)
					{
						sscanf(msg_tcp.message, "%s %s", command, topic);
						int found = 0, j, k;
						// caut pe ce pozitie se afla topic-ul in lista de topic-uri
						// cu abonati
						for (j = 0; j < (int)topics.size(); j++)
							if (strcmp(topics.at(j).c_str(), topic) == 0)
							{
								found = 1;
								break;
							}
						// elimin clientul din lista de abonati pentru topic-ul primit
						(subscribers_of_topic.at(j)).erase(subscribers_of_topic.at(j).begin() + j - 1);
					}
					else if (strncmp(msg_tcp.message, "exit", 4) == 0)
					{
						close(i);
						printf("Client %s disconnected.\n", msg_tcp.id_client);

						// elimin clientul care s-a deconectat de la vectorul
						// celor conectati
						for (int j = 0; j < connected.size(); j++)
							if (connected.at(j).sockfd == i)
								connected.erase(connected.begin() + j);
						FD_CLR(i, &read_fds);
					}
					else
						continue;
				}
			}
		}
	}

	// inchid socket-ii
	close(sockfd_udp);
	close(sockfd_tcp);
	return 0;
}
