#include<stdio.h>
#include<stdint.h>
#include<stdlib.h>
#include<string.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <math.h>
#include <assert.h>
#include"p2mpserver.h"

segment curr_pkt,*recv_buffer;
int curr_pkt_seq_num;
//segment *recv_ack_buffer;
//int flag;

int last_packet,last_pkt_seq_num;
int n;
double prob;
FILE *file;
int server_port;
float p;
int buf_counter;
uint32_t next_seg_seq_num;
int soc,send_soc;
struct sockaddr_in sender_addr;

void cleanup()
{
	free(recv_buffer);
	shutdown(soc,SHUT_RDWR);
	close(soc);
}

double generate_random_Probability()
{
        long int seed;
        int rndm,next_free;
        struct timeval ct;

        gettimeofday(&ct, NULL);
        seed = (ct.tv_usec)*RAND_MAX + rand()%RAND_MAX;
        srand(seed);	
        //return (double)( (rand()%(end - start)) + start ) ;
	return ((double)(rand())/((double)RAND_MAX+1) * 5);
}




uint16_t compute_checksum(char *data)
{
	uint16_t padd=0; //in case data has odd no. of octets
	uint16_t word16; //stores 16 bit words out of adjacent 8 bits
	uint32_t sum;
	int i;

	int len_udp = strlen(data)+strlen(data)%2; //no. of octets
	sum=0;

	for (i=0; i<len_udp; i=i+2)
	{
        	word16 =((data[i]<<8)&0xFF00)+(data[i+1]&0xFF);
	        sum = sum + (unsigned long)word16;
	}

	// keep only the last 16 bits of the 32 bit calculated sum and add the carries
	while (sum>>16)
	sum = (sum & 0xFFFF)+(sum >> 16);

	// Take the one's complement of sum
	sum = ~sum;

	//printf("Checksum is %x\n",(uint16_t)sum);
	return ((uint16_t) sum);

}

int init_recv_window()
{
	recv_buffer = (segment *)malloc(n*sizeof(segment));
	//HIDEprintf("Allocated %d segments of Total length: %d\n",n,(int)(n*sizeof(segment)));
	next_seg_seq_num = 0;	//Indicates that the next expected seq no. is 0
	buf_counter = 0; //not used`
	last_pkt_seq_num = -2;
}

int init_receiver(int argc, char *argv[])
{
        if(argc<=1 || argc>5)
        {
                printf("Incorrect command line agruments\n");
                exit(-1);
        }

        n = atoi(argv[3]);
        printf("Window size:%d\n",n);
        if(!(file = fopen(argv[2],"wb")))
        {
                printf("File opening failed\n");
                exit(-1);
        }
        server_port = atoi(argv[1]);
        printf("Server port is: %d\n",server_port);


	prob = 0.0f;	
	prob = atof(argv[4]);
	printf("Loss Probability is set to: %f\n",prob);

	struct sockaddr_in my_addr;    // my address information

	if ((soc = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        	printf("\n error creating socket");
 	        exit(-1);
	}

	my_addr.sin_family = AF_INET;         // host byte order
	my_addr.sin_port = htons(server_port);// short, network byte order
	my_addr.sin_addr.s_addr = INADDR_ANY; // automatically fill with my IP
	memset(my_addr.sin_zero, '\0', sizeof my_addr.sin_zero);

	if (bind(soc, (struct sockaddr *)&my_addr, sizeof my_addr) == -1) {
		printf("\n bind failed");
		exit(-1);
	}
	
	init_recv_window();
	last_packet = 1; //not used
}


int udt_send(int seg_index)
{
//        struct sockaddr_in their_addr; // connector's address information
        struct hostent *he;
        int numbytes;
        int addr_len;
        int len;
        char buf[MAXLEN];
	int ack_pkt_type;
//HIDE	printf("Seg index is %d\n",seg_index);

        strcpy(buf,"");
	
	buf[0]='\0'; //CHANGED

	//SEND THE ACK For Current Packet (seg_index)
	//Right now overrighting the recv_buffer's pkt_type to indicate that it is now an ACK Packet (Logically) ..
//	recv_buffer[seg_index%n].seq_num = seg_index;
//	seg_index = seg_index%n;
//THE MISTAKE	if(seg_index>0)
		//HIDEprintf("In UDT SEND: Seg index is: %d\t Seq Num is %d\n",seg_index,recv_buffer[seg_index%n].seq_num);
//	printf("Pkt type in receive buffer is : %x", (uint16_t)recv_buffer[seg_index].pkt_type);
	if(last_pkt_seq_num == seg_index)
	{	
			
		
		ack_pkt_type = 0X00AA;  //indicates ACK for last packet

		sprintf(buf,"%d\n%d\n%d\n",seg_index,0,ack_pkt_type);

		len = strlen(buf);
	        //HIDEprintf("\n\n***********ACK Bytes Sent: %d\n\n",len);
	        if (sendto(soc,buf, len, 0, (struct sockaddr *)&sender_addr, sizeof (sender_addr)) == -1) {
                //HIDEprintf("Error in sending");
                exit(-1);
		}
		else{
                      //HIDE  printf("\nACK sent for last packet in file ... closing output file and exiting ... \n\n");
			fclose(file);
			//free(recv_buffer);
                        pthread_exit(NULL);
                }
        }
	else{
                ack_pkt_type = 0XAAAA;  //indicates Normal ACK

                sprintf(buf,"%d\n%d\n%d\n",seg_index,0,ack_pkt_type);

                len = strlen(buf);
                //HIDEprintf("\n\n***********ACK Bytes Sent: %d\n\n",len);
                if (sendto(soc,buf, len, 0, (struct sockaddr *)&sender_addr, sizeof (sender_addr)) == -1) {
                	printf("Error in sending");
               		exit(-1);
		}

	}
        
}


//Sending ack for the curr_pkt_seq_num  (wrapped around to get correct 'recv_buffer' index)  MSS in 'recv_buffer'
int send_ack(int seg_index)
{
//		int index = seg_index%n;
		//HIDEprintf("\nSent ACK For Packet: %d",seg_index);
		udt_send(seg_index);

}

int compare_checksum(uint16_t checksum)
{
	if(curr_pkt.checksum == checksum)
		return TRUE;
	return FALSE;
}


int add_to_buffer(int index)
{
//        printf("Index is %d\n",index);
	
		
        recv_buffer[index].seq_num = curr_pkt.seq_num;

	//ARRIVED INDICATES current packet is arrived
	recv_buffer[index].arrived = TRUE;

/*        if((recv_buffer[index]rdata = (char*)malloc(strlen(curr_pkt.data)*sizeof(char)))==NULL){
                perror("CANNOT ALLOCATE MEMORY :( : Add to Buffer \n");
                exit(1);
        }*/
        strcpy(recv_buffer[index].data,curr_pkt.data);
        //strcat(recv_buffer[index].data,"\0"); 
	
//HIDE	printf("\nThe index in the buffer where the segment is buffered is : %d\n",index);
        //printf("Length of data in packet with seq_num=%d DATA: %d\n",curr_pkt.seq_num,(int)strlen(curr_pkt.data));
	curr_pkt_seq_num = curr_pkt.seq_num;

	curr_pkt.data[0] = '\0';
/*	if(flag)
	{
	        //free(curr_pkt.data);
		curr_pkt.data == NULL;
		flag = 0;
		//printf("freed!!!\n");
	}*/
        return TRUE;
}

int udt_recv()
{
	char *buf;
	int numbytes,data_length;
	char *a, *b, *c, *d, *e,*f;
	//flag = 0; 
	int addr_len = sizeof (sender_addr);
	//strcpy(buf,"");
	buf = (char*)calloc(MAXLEN,sizeof(char));

//	buf = (char *)malloc(MAXLEN*sizeof(char));
//	buf[0]='\0';
	//printf("\nIN UDT RECV - Receiving Data...\n");

	numbytes=recvfrom(soc, buf, MAXLEN , 0,(struct sockaddr *)&sender_addr, &addr_len);
	if(numbytes == -1 || numbytes == 0) {
		printf(" Error in receiving\n");
		//EOF condition.
		fclose(file);
		//exit(-1);
		pthread_exit(NULL);
	}
	//HIDEprintf("\n\n***********Bytes Received: %d\n\n",numbytes);	
//	printf("the received string is%s\n",buf);
//	buf[strlen(buf)] = '\0';
	//d[0] = '\0';
	//curr_pkt.data[0] = '\0';
	
	//printf("\n'd' = %s\n",d);
	a = strtok_r(buf,"\n",&d);
	curr_pkt.seq_num = (uint32_t)atoi(a);

//	printf("\n'd' = %s\n",d);
	b = strtok_r(NULL,"\n",&d);
	curr_pkt.checksum = (uint16_t)atoi(b);

//	printf("\n'd' = %s\n",d);
	c = strtok_r(NULL,"\n",&d);
	curr_pkt.pkt_type = (uint16_t)atoi(c);
	if(curr_pkt.pkt_type == 0x5500){
		last_pkt_seq_num = curr_pkt.seq_num;
		//HIDEprintf("Last Packet Seq nUm set to: %d\n",last_pkt_seq_num);
	}

	//printf("\nPacket type in received packet is : %d\n",curr_pkt.pkt_type);

	strcpy(curr_pkt.data,d);
	fflush(stdout);

	//HIDEprintf("seq_num: %d, checksum: %x, packet type: %x\n",curr_pkt.seq_num,curr_pkt.checksum,curr_pkt.pkt_type);
//	printf("For copied data.. length is %d\n data is %s\n\n",(int)strlen(curr_pkt.data),curr_pkt.data);
	strcpy(d,"");	
	d[0]='\0';
	free(buf);
}


int is_next_expected()
{
	if(curr_pkt.seq_num == next_seg_seq_num) //In Sequence TRUE
		return TRUE;
	else
		return FALSE;
}

/*int is_gap_filled()
{
	if(recv_buffer[(curr_pkt.seq_num%n)-1].seq_num == curr_pkt.seq_num-1 && recv_buffer[(curr_pkt.seq_num%n)+1].seq_num == curr_pkt.seq_num+1)
		return TRUE;
	return FALSE;
}*/

int is_in_recv_window()
{

	//HIDEprintf("\nCurr pkt = %d, Next_seg num = %d\n",curr_pkt.seq_num,next_seg_seq_num);
	if(curr_pkt.seq_num<next_seg_seq_num)//recv receives a packet in some previous window
		return -1;
	else if(curr_pkt.seq_num<next_seg_seq_num+n)
	{
		return curr_pkt.seq_num%n;//-next_seg_seq_num+1;
	}
	return n;
}

//writing 1 MSS Data into file .. called after checksum comparison
int write_file(char *temp_buf)
{
    	int j=0,numbytes;
	char buff[1];
	//HIDEprintf("\nWritten! %d bytes",(int)strlen(temp_buf));
	if((numbytes=fwrite(temp_buf,strlen(temp_buf),1,file))<0)
	{
		perror("\nWrite error");
		exit(1);
	}
	return numbytes;
}

int process_pkt()
{
	if(is_next_expected()){
		return 1; //
	}
	else{  //Curr Pkt is not in sequence
	//	if((is_gap_filled() && !is_next_expected()) || (!is_gap_filled() && !is_next_expected())) //Gap is filled but the packet is out of sequence
	//	{
			//Buffer the curr_pkt
			return 2;
			
	//	}
	}
	return 0;
       // while (is_in_sequence())
		//printf("Received packet is:\nSeq Number:%d\nChecksum:%d\npacket Type:%d\ndata: %s\n",curr_pkt.seq_num,curr_pkt.checksum,curr_pkt.pkt_type,curr_pkt.data);
                //write_file();
}

void slide_window()
{
	next_seg_seq_num++;
}


// Usable AlmostEqual function .. NOT USING IT RIGHT NOW!
int AlmostEqual2sComplement(float A, float B, int maxUlps)
{
    // Make sure maxUlps is non-negative and small enough that the
    // default NAN won't compare as equal to anything.
    assert(maxUlps > 0 && maxUlps < 4 * 1024 * 1024);
    int aInt = *(int*)&A;
    // Make aInt lexicographically ordered as a twos-complement int
    if (aInt < 0)
        aInt = 0x80000000 - aInt;
    // Make bInt lexicographically ordered as a twos-complement int
    int bInt = *(int*)&B;
    if (bInt < 0)
        bInt = 0x80000000 - bInt;
    int intDiff = abs(aInt - bInt);
    if (intDiff <= maxUlps)
        return TRUE;
    return FALSE;
}


int recv_data()
{
        int ret_val,process_ret;
        uint16_t recv_checksum;

	//Got a new packet
//	printf("Calling UDT RECV...\n");
	udt_recv();
	
	double sample = generate_random_Probability();
//	printf("Random Prob Generated: %f\n",sample);
	if( sample <=  prob ) //Generated random probability is < Set Loss Probability
	{
		printf("Packet Loss, Sequence Number: %d\n",curr_pkt.seq_num);
		return FALSE;
	}
	recv_checksum = compute_checksum(curr_pkt.data);

	if(!(compare_checksum(recv_checksum)))
	{
//		printf("Checksum mismatch\n");
               return FALSE; //Discard and no ACK Sent
	}

	ret_val = is_in_recv_window();
	//printf("^^^^^^^Ret Val in RECV DATA IS: %d\n",ret_val);
        if(ret_val == -1)
	{
               // send ack for last correctly received packet 
	       send_ack(next_seg_seq_num-1);
	       return TRUE;
		  
	}
        else if(ret_val == n)
                return FALSE;

	process_ret = process_pkt();
	if(process_ret == 1) //Add to Buffer, slide the window, send ACK
	{
		//add_to_buffer(ret_val-1);
		add_to_buffer(ret_val);
	
		int i;	
		for(i = next_seg_seq_num%n;recv_buffer[i].arrived == TRUE;i=(i+1)%n){
	//for(i=0;recv_buffer[i].arrived ==TRUE;i=(i+1)%n){
			//HIDEprintf("Before slide window\n");
			slide_window();
			write_file(recv_buffer[i].data); //when the window slides, write the corresponding segment to file
			recv_buffer[i].arrived = FALSE;
			//HIDEprintf("i is %d\n",i);
			//FLUSH i'th index in recv_buffer
		}
		//HIDEprintf("Sending ack\n");
		//send ACK for curr_pkt.seq_num
		send_ack(next_seg_seq_num-1);
		
	}
	else if(process_ret == 2) //Add to Buffer, send ACK for most recent in-sequence packet
	{
		add_to_buffer(ret_val);
		//send ACK for most in-sequence packet ... next_seg_seq_num
		send_ack(next_seg_seq_num-1);
	}

	//Write this 1 MSS to file
//	write_file(curr_pkt.data);
        return TRUE;
}

