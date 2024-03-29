#include"utils.h"

t_log* logger;

int iniciar_servidor(void)
{
	/* < --- Nota sobre lo de abajo --- >

	Bueno... primero, hints es una estructura addrinfo que almacena informacion sobre la comunicacion 
	entre el servidor y las demas partes. Asi yo puedo "configurar" la comunicacion. En este caso, "memset" setea los valores de hints en 0
	AF_UNSPEC se usa para denotar que es IPv4, SOCK_STREAM para indicar que usa TCP, y AI_PASSIVE para no tener que hardcoder la IP de la vm
	en la que corra el servidor. El getaddrinfo usa esta estructura hints para poder buscar conexiones adecuados y cargarlos en una lista (servinfo). 
	Como lo hace? No tengo ni idea, y tampoco se exactamente que es lo que hay en ese nodo... pero esa lista tiene direcciones que se pueden 
	utilizar para establecer una conexion. Lo ideal seria utilizar *p y un codigo auxiliar que me permite recorrer la lista (esta en la guia de beej) 
	e ir tanteando si alguno de esos sockets son utilizables. En caso de serlo, se utiliza, y sino devuelve NULL y se manejan adecuadamente los errores. 
	
	Este es el famoso addrinfo... 

	struct addrinfo {
    int              ai_flags;     // AI_PASSIVE, AI_CANONNAME, etc.
    int              ai_family;    // AF_INET, AF_INET6, AF_UNSPEC
    int              ai_socktype;  // SOCK_STREAM, SOCK_DGRAM
    int              ai_protocol;  // usá 0 para "cualquiera"
    size_t           ai_addrlen;   // tamaño de ai_addr en bytes
    struct sockaddr *ai_addr;      // struct sockaddr_in o _in6
    char            *ai_canonname; // hostname canónico completo

    struct addrinfo *ai_next;      // lista enlazada, próximo nodo
	};	

	struct sockaddr {
    unsigned short    sa_family;    // familia de direcciones, AF_xxx
    char              sa_data[14];  // 14 bytes de dirección de protocolo
	}; 

	Como funciona socket()? -> Bueno, tecnicamente, un socket seria como una interfaz entre el client y el server para poder 
	comunicarnos. Ambas partes lo ejecutan. El cliente crea un socket directamente para comunicarse con el servidor, y el servidor crea
	el suyo para poder activamente escuchar potenciales conexiones. Una vez que se identifica alguna, se crea un socket para ese cliente, que se va a 
	utilizar para establecer la comunicacion. Importante! socket() devuelve un valor "int"
	Como funciona bind()? -> Bind lo que hace es asociar ese socket que creamos a un puerto especifico. Esto nos permite poder despues usar el
	listen en un puerto particular donde ya tenemos este socket configurado.
	Como funciona listen() -> El nombre lo dice todo, espera conexiones por parte del cliente, mientras se toma un te

	*/

	int socket_servidor;

	struct addrinfo hints, *servinfo, *p;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	getaddrinfo(NULL, PUERTO, &hints, &servinfo);

	// Creamos el socket de escucha del servidor -> Aca se usa la informacion propia de la conexion que se pudo utilizar de la lista 
	// (suponiendo que se pudo utilizar alguna), ya que de lo contrario devolveria NULL, y tendriamos que manejar los errores.
	// Tecnicamente (Again) aca es donde deberiamos recorrer la lista e ir probando cada posible conexion... pero eso es otra historia
	socket_servidor = socket(servinfo->ai_family,
                    servinfo->ai_socktype,
                    servinfo->ai_protocol);

	// Asociamos el socket a un puerto. Aca estaria bueno volver arriba y ver que el ai_addr es literalmente la direccion IP, y bueno... el otro es el length de la misma
	bind(socket_servidor, servinfo->ai_addr, servinfo->ai_addrlen);

	// Escuchamos las conexiones entrantes. La ultima, lo prometo. Esto es basicamente hacer que el servidor quede a la espera de una conexion con el cliente
	// SOMAXCONN es la cantidad maxima de conexiones que puede aceptar. No deberian ser muchas
	listen(socket_servidor, SOMAXCONN);

	freeaddrinfo(servinfo);
	log_trace(logger, "Listo para escuchar a mi cliente");

	return socket_servidor;
}

int esperar_cliente(int socket_servidor)
{
	/*  

	Aceptamos un nuevo cliente. Previamente para esto tiene que haber una solicitud de un cliente. Lo importante es que todas estas syscalls se detienen
	cuando se ejecutan. Es decir, si yo desde el cliente hago un connect(), no va a seguir el flujo de ejecucion hasta que haya un respuesta
	del servidor. Por esto se dice que son "bloqueantes". Si tenemos en cuenta el orden, primero debe haber un listen, despues un
	connect, y un accept para aceptar esa conexion... De esta forma, el accept me retorna el glorioso 
	Socket para establecer la tan esperada conexion con el cliente

	Como funciona accept() -> Accept cumple la unica pero tan importante funcion de crear el socket de conexion con el cliente una vez que esa conexion llego
	Es importante destacar que los 2 valores NULL de la syscall deberian ser la IP y su longitud. Al indicar NULL, le decimos que no nos interesa de donde venga,
	que acepte la conexion. 

	*/

	int socket_cliente;

	socket_cliente = accept(socket_servidor, NULL, NULL);

	log_info(logger, "Se conecto un cliente!");

	return socket_cliente;
}

int recibir_operacion(int socket_cliente)
{
	/* 
	Como para todo free hay un malloc, para todo send tiene que haber un recv(). 
	
	*/

	int cod_op;
	if(recv(socket_cliente, &cod_op, sizeof(int), MSG_WAITALL) > 0)
		return cod_op;
	else
	{
		close(socket_cliente);
		return -1;
	}
}

void* recibir_buffer(int* size, int socket_cliente)
{
	void * buffer;

	recv(socket_cliente, size, sizeof(int), MSG_WAITALL);
	buffer = malloc(*size);
	recv(socket_cliente, buffer, *size, MSG_WAITALL);

	return buffer;
}

void recibir_mensaje(int socket_cliente)
{
	int size;
	char* buffer = recibir_buffer(&size, socket_cliente);
	log_info(logger, "Me llego el mensaje %s", buffer);
	free(buffer);
}

t_list* recibir_paquete(int socket_cliente)
{
	int size;
	int desplazamiento = 0;
	void * buffer;
	t_list* valores = list_create();
	int tamanio;

	buffer = recibir_buffer(&size, socket_cliente);
	while(desplazamiento < size)
	{
		memcpy(&tamanio, buffer + desplazamiento, sizeof(int));
		desplazamiento+=sizeof(int);
		char* valor = malloc(tamanio);
		memcpy(valor, buffer+desplazamiento, tamanio);
		desplazamiento+=tamanio;
		list_add(valores, valor);
	}
	free(buffer);
	return valores;
}
