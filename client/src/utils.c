#include "utils.h"


void* serializar_paquete(t_paquete* paquete, int bytes)
{
	/* ¿Que hace esta funcion? 
	Basicamente hace ese STREAM adicional que se menciona abajo. Aca si necesitamos el offset.

	Un detalle importante de la guia es que usa esta linea para cargar el stream en el STREAM adicional :  
	memcpy(a_enviar + offset, paquete->buffer->stream, paquete->buffer->size);
	Y si bien antes dije que no se podian pasar punteros y aca parezca que lo esta haciendo, no lo esta haciendo. Ya que lo que esta pasando
	son los bytes de cada dato. Cuando lo reciba, tendre que crear un puntero y deserializar. 
	Bueno, aca la ultima linea hace lo mismo. Carga todo lo necesario en el magic, y lo devuelve. Para poder usar el send de forma correcta
	
	Hay que tener en cuenta que siempre sigue un formato y es: Codigo de operacion, size del stream y recien ahi el stream (que en realidad son los datos "Puros" recibidos). 
	A estos datos puros se le conoce tambien como PAYLOAD

	Otro detalle, yo podria simplemente crear este puntero void sin el paquete y deberia funcionar igual, pero creo que a efectos
	de mantener la prolijidad es mejor tener los structs del buffer y del paquete...

	*/
	void * magic = malloc(bytes);
	int desplazamiento = 0;

	memcpy(magic + desplazamiento, &(paquete->codigo_operacion), sizeof(int));
	desplazamiento+= sizeof(int);
	memcpy(magic + desplazamiento, &(paquete->buffer->size), sizeof(int));
	desplazamiento+= sizeof(int);
	memcpy(magic + desplazamiento, paquete->buffer->stream, paquete->buffer->size);
	desplazamiento+= paquete->buffer->size;

	return magic;
}

int crear_conexion(char *ip, char* puerto)
{
	struct addrinfo hints;
	struct addrinfo *server_info;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	int result = getaddrinfo(ip, puerto, &hints, &server_info);

	if(result != 0) {
		printf("Malio sal");
		exit(1);
	}

	// Ahora vamos a crear el socket.
	int socket_cliente = 0;
	socket_cliente = socket(server_info->ai_family, server_info->ai_socktype, server_info->ai_protocol);

	// Ahora que tenemos el socket, vamos a conectarlo. Al ser bloqueante tambien queda a la espera de la respuesta del servidor.
	// Lo que hace es conectar al cliente y con el servidor usando la informacion de la lista servinfo y si conecta asigna el socket. Supongo que aca tambien deberia haber un manejo de errores
	connect(socket_cliente, server_info->ai_addr, server_info->ai_addrlen);

	freeaddrinfo(server_info);

	return socket_cliente;
}

void enviar_mensaje(char* mensaje, int socket_cliente)
{
	/* 
	Bueno, aca vamos por partes. Primero, Declaramos y hacemos malloc para nuestro paquete
	con un codigo de operacion de MENSAJE. Aca es donde podria ir cualquier enum que se pueda llegar a utilizar
	Es importante entender que cada paquete puede serializar un tipo de dato distinto (distintos punteros a un struct)
	por lo que no seria nada raro que tenga 500 lineas de codigo de seralizacion en el TP, cada una por cada dato a enviar (espermos que no)
	Lo proximo a hacer es guardar espacio para el buffer, y luego su size. En este caso el size es simplemente el strlen(mensaje) +1 porque es un string...
	peeero si enrealidad quisieramos el size de un struct seria algo asi : 

	buffer->size = sizeof(uint32_t) * 3 // DNI, Pasaporte y longitud del nombre
             + sizeof(uint8_t) // Edad
             + persona.nombre_length; // Ya que hay variables que son dinamicas y debemos incluir el tamaño. Esto tambien es importante **

	** El string serializado abajo es estatico, es decir, yo ya se el tamaño. Pero si el dia de mañana tengo un nombre asignado dinamicante, tenemos que definir una variable 
	para ello, sino la vm que lo reciba no va a saber cuanta memoria asignarle.
	Otra pregunta boluda, porque no tengo un offset en el struct como lo tengo en el ejemplo de la guia? Bueno, porque aca no vamos a usar offset para cargar los datos al buffer del paquete
	ya que es solo un string, peeero... si fueran varios datos contenidos dentro de un struct, logicamente los necesitaria. 
	Otra mas, que cantidad de bytes esta realmente reservando esto? int bytes = paquete->buffer->size + 2*sizeof(int); ----> Reserva para el size del buffer (osea, el tamaño del stream), 
	y 2 sizeof(int) para el mismo largo del size y otro para el codop. De esta forma yo puedo ahora crear mi stream para enviar. Para esto, llamo a la func de arriba serializar,
	con la cantidad de bytes de recien y el paquete mismo armado (que ya tiene el codop, el buffer->size y el buffer->stream con todos los valores, en este caso, un string (mensaje))

	IMPORTANTE! Porque no se envia el paquete directo y se crea otro buffer adicional (void*) cuando se llama a serializar. Bueno, eso es porque no podemos enviar un puntero, porque 
	el padding no es el mismo en todos los dispositivos y eso causaria problemas y perdidas de datos. Por eso se crea un STREAM adicional

	Por ultimo, pero no menos importante, el send. Lo unico que hace es enviar los datos con ese STREAM adicional y los bytes correspondientes
	Tranquilamente podria reemplazar bytes en send por -> paquete->buffer->size + 2*sizeof(int); (en la guia no usa una variable adx como aca)

	En definitiva, siempre se pasa todo a un buffer para despues pasarlo a un stream. Lo ideal serai a partir de esto 
	crear algunas TDA para que sea mas facil hacerlo todo el tiempo (guia)

	typedef struct {
	int size; void* stream;
	} t_buffer;

	typedef struct {
	op_code codigo_operacion;	t_buffer* buffer;
	} t_paquete;

	*/

	t_paquete* paquete = malloc(sizeof(t_paquete));

	paquete->codigo_operacion = MENSAJE;
	paquete->buffer = malloc(sizeof(t_buffer));
	paquete->buffer->size = strlen(mensaje) + 1;
	paquete->buffer->stream = malloc(paquete->buffer->size);
	memcpy(paquete->buffer->stream, mensaje, paquete->buffer->size);

	int bytes = paquete->buffer->size + 2*sizeof(int);

	void* a_enviar = serializar_paquete(paquete, bytes);

	send(socket_cliente, a_enviar, bytes, 0);

	free(a_enviar);
	eliminar_paquete(paquete);
}

// Estas funciones basicamente cumplen el mismo rol que para el logger el log_create, o para el config el config_create. Si yo no tengo un 
// Paquete creado, no puedo enviar ningun mensaje

void crear_buffer(t_paquete* paquete)
{
	paquete->buffer = malloc(sizeof(t_buffer));
	paquete->buffer->size = 0;
	paquete->buffer->stream = NULL;
}

t_paquete* crear_paquete(void)
{
	t_paquete* paquete = malloc(sizeof(t_paquete));
	paquete->codigo_operacion = PAQUETE;
	crear_buffer(paquete);
	return paquete;
}


// Esto simplemente agrega el string al stream del buffer. La diferencia es que yo aca puedo guardar muchos strings
void agregar_a_paquete(t_paquete* paquete, void* valor, int tamanio)
{
	paquete->buffer->stream = realloc(paquete->buffer->stream, paquete->buffer->size + tamanio + sizeof(int));

	memcpy(paquete->buffer->stream + paquete->buffer->size, &tamanio, sizeof(int));
	memcpy(paquete->buffer->stream + paquete->buffer->size + sizeof(int), valor, tamanio);

	paquete->buffer->size += tamanio + sizeof(int);
}

// Y esto lo envia
void enviar_paquete(t_paquete* paquete, int socket_cliente)
{
	int bytes = paquete->buffer->size + 2*sizeof(int);
	void* a_enviar = serializar_paquete(paquete, bytes);

	send(socket_cliente, a_enviar, bytes, 0);

	free(a_enviar);
}

void eliminar_paquete(t_paquete* paquete)
{
	free(paquete->buffer->stream);
	free(paquete->buffer);
	free(paquete);
}

void liberar_conexion(int socket_cliente)
{
	close(socket_cliente);
}
