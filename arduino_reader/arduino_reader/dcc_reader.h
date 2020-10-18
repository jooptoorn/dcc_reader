#ifdef __cplusplus
extern "C" {
#endif

#define TR1MIN 52ul
#define TR1MAX 64ul
#define TR1DMAX 12ul //officially 6us. However, Arduino resolution is 4us which not high enough to support this check. Disable by making equal to 12us. 

#define TR0MIN 90ul
#define TR0MAX 10000ul
#define TR0TOTMAX 12000ull

#define NUM_ONES_VALID_PREAMBLE 12

#define MAX_BYTESIZE_ADDR 2
#define MAX_BYTESIZE_DATA 7
#define MAX_BYTESIZE_CMD_ARG 4

#define DCC_14_MODE 0   //1

typedef enum dcc_reader_msg_type {
    dcc_reader_error = -1,  //Error status
    no_new_dcc_msg = 0,     //no new message
    dcc_msg_dcci,           //000 Decoder and Consist Control Instruction
    dcc_msg_aoi,            //001 Advanced Operation Instructions
    dcc_msg_sdi,            //010 Speed and Direction Instruction for reverse operation & 011 Speed and Direction Instruction for forward operation 
    dcc_msg_fgi1,           //100 Function Group One Instruction 
    dcc_msg_fgi2,           //101 Function Group Two Instruction 
    dcc_msg_fexp,           //110 Future Expansion 
    dcc_msg_cvai,           //111 Configuration Variable Access Instruction
    dcc_msg_idle            //idle message
} dcc_reader_msg_type_t;

// state machine enumeration
typedef enum dcc_reader_state {
    reader_reset = 0,
    read_preamble,
    read_start,
    read_byte,
    read_sync,
    check_crc,
} dcc_reader_state_t;

typedef enum dcc_halfbit_state {
    halfbit_uninitialized = 0,
    half_bit,
    valid_1,
    valid_0,
    invalid_bit
} dcc_halfbit_t;

typedef struct dcc_message{
    unsigned int addr;
    unsigned int cmd;
    dcc_reader_msg_type_t msg_type;
    unsigned char cmd_arg[MAX_BYTESIZE_CMD_ARG];
    short int speed;
    unsigned char af_group1;
    unsigned char af_group2;
    unsigned char validMsg;
} dcc_message_t;


dcc_halfbit_t dcc_feed_halfbit(unsigned long half_bit_time);

dcc_reader_msg_type_t dcc_feed_bit(dcc_halfbit_t bit, dcc_message_t* msg_buf);

void dcc_interpret_msg(unsigned char* data, dcc_message_t* msg_buf, unsigned int len);

#ifdef __cplusplus
} // extern "C"
#endif