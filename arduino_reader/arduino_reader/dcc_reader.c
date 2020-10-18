#include "dcc_reader.h"

dcc_halfbit_t dcc_feed_halfbit(unsigned long half_bit_time)
{
    static dcc_halfbit_t state = halfbit_uninitialized;
    static unsigned long prev_half_bit_time = 0;

    if(state == halfbit_uninitialized)
    {
        //in this state we are waiting for the transition between a 1 and 0 to sync
        if( prev_half_bit_time  >= TR1MIN    && prev_half_bit_time  <= TR1MAX && 
            half_bit_time       >= TR0MIN    && half_bit_time       <= TR0MAX)
        {
            //this is the right transition, period A of a zero-bit was found
            state = half_bit;
        }
    } //end if(initializing)
    else if(state == half_bit)
    {
        //period A was already received are we processing a one or a zero?
        if( prev_half_bit_time >= TR1MIN && prev_half_bit_time <= TR1MAX )
        {
            //one.

            //perform following checks
            //1. period B is also valid
            //2. difference between period A and B is valid
            
            unsigned long delta_bit_time;
            if(half_bit_time > prev_half_bit_time)
                delta_bit_time = half_bit_time - prev_half_bit_time;
            else
                delta_bit_time = prev_half_bit_time - half_bit_time;
            
            //perform checks 1 and 2
            if(half_bit_time >= TR1MIN && half_bit_time <= TR1MAX && delta_bit_time <= TR1DMAX)
            {
                //it is a valid one-bit
                state = valid_1;
            }
            else
            {
                //invalid bit was received
                state = invalid_bit;
            }
            
        }
        else if (prev_half_bit_time >= TR0MIN && prev_half_bit_time <= TR0MAX)
        {
            //zero

             //perform following checks
            //1. period B is also valid
            //2. total of period A and B is valid

             unsigned long long total_bit_time = half_bit_time + prev_half_bit_time;

            //perform checks 1 and 2
            if(half_bit_time >= TR0MIN && half_bit_time <= TR0MAX && total_bit_time <= TR0TOTMAX)
            {
                //it is a valid zero-bit
                state = valid_0;
            }
            else
            {
                //invalid bit was received
                state = invalid_bit;
            }

        }
    } //end if(halfbit)
    else if(state == valid_0 || state == valid_1)
    {
        // we have now received a new period A. Perform sanity check
        if(half_bit_time >= TR0MIN && half_bit_time <= TR0MAX)
        {
            //received first half of a zero-bit
            state = half_bit;
        }
        else if(half_bit_time >= TR1MIN && half_bit_time <= TR1MAX)
        {
            //received first half of a one-bit
            state = half_bit;
        }
        else
        {
            //received invalid sequence
            state = invalid_bit;
        }
    } //end if(receiving period A)
    else
    {
        //this only happens for invalid bits, reset statemachine
        state = halfbit_uninitialized;
    }

    // store bit time for next half-cycle
    prev_half_bit_time = half_bit_time;

    return state;
}

dcc_reader_msg_type_t dcc_feed_bit(dcc_halfbit_t bit, dcc_message_t* msg_buf)
{
    static dcc_reader_state_t dcc_reader_state = reader_reset;
    static unsigned char data[MAX_BYTESIZE_DATA];
    static unsigned char crc;
    static unsigned int rx_byte_cnt = 0;
    
    //check if we are processing any valid bits at all
    if(bit == halfbit_uninitialized || bit == invalid_bit)
    {
        //reset state on next cycle
        dcc_reader_state == reader_reset;
        //report the error
        return dcc_reader_error;
    }
    //return early if there is no full bit to process
    if(bit == half_bit)
        return no_new_dcc_msg;

    //reset after errors or completed transfers
    if(dcc_reader_state == reader_reset)
    {
        //reset state vars
        memset(data, 0, sizeof(data));
        crc = 0;
        rx_byte_cnt = 0;

        //always progress to next state
        dcc_reader_state = read_preamble;
    }

    //a valid bit was received, process the new data
    if(dcc_reader_state == read_preamble)
    {
        // in this state, check for valid preamble that consists of more than 12 valid one-bits
        static unsigned long int num_valid_ones = 0;
        if(bit == valid_1)
            num_valid_ones++;
        else
            num_valid_ones = 0;
        
        //check if we have received enough valid full 1 bits
        if(num_valid_ones > NUM_ONES_VALID_PREAMBLE)
        {
            //bingo. Reset vars and goto next state
            num_valid_ones = 0;
            dcc_reader_state = read_start;
        }

    }
    else if(dcc_reader_state == read_start)
    {
        // in this state, simply wait until we encounter a 0-bit to indicate dataframe read_start
        if(bit == valid_0){
            dcc_reader_state = read_byte;
        }
    }
    else if(dcc_reader_state == read_byte)
    {
        // in this state, receive 8 bits before checking a sync bit
        static unsigned short rx_bit_cnt = 0;
        //shift and add new data
        data[rx_byte_cnt] = data[rx_byte_cnt] << 1;
        if(bit == valid_1)
            data[rx_byte_cnt] |= 1;

        //increase counter until there are 8 bits read_start
        if(++rx_bit_cnt > 7)
        {
            //the byte is completely read. Wait for sync bit now.

            //sanity check data frame size
            if(++rx_byte_cnt > MAX_BYTESIZE_DATA-1)
            {
                //this should never happen. Abort reception
                dcc_reader_state = reader_reset;
                return dcc_reader_error;
            }

            //all is fine, goto next state
            rx_bit_cnt = 0;
            dcc_reader_state = read_sync;
        }

    }
    else if(dcc_reader_state == read_sync)
    {
        //in this state we must receive a valid 0 to receive another data byte. The transaction is finished is there is a valid 1-bit.
        if(bit == valid_0)
        {
            //packet sync-bit was received, receive another data byte
            dcc_reader_state = read_byte;
        }
        else if(bit == valid_1)
        {
            //packet end-bit was received, goto crc check
            dcc_reader_state = check_crc;
        }
        else
        {
            //can never end up here
            dcc_reader_state = reader_reset;
            return dcc_reader_error;
        }
        
    }

    if(dcc_reader_state == check_crc)
    {
        //always reset reader when end up here, ready to receive next message
        dcc_reader_state = reader_reset;
        
        //check for CRC, which is XOR of all bytes received:
        unsigned char crc = 0;
        for(unsigned int i = 0; i < rx_byte_cnt - 1; i++)
         crc ^= data[i];

        //check if last byte is same as calculated crc
        if(crc != data[rx_byte_cnt-1])
        {
            return dcc_reader_error;
        }

        /* interpret message */
        dcc_interpret_msg(data, msg_buf, rx_byte_cnt-1);
        return msg_buf->msg_type;
    }

    return no_new_dcc_msg;
}

void dcc_interpret_msg(unsigned char* data, dcc_message_t* msg_buf, unsigned int len)
{
    //reset this message
    memset(msg_buf,0,sizeof(msg_buf));
    //assume it is valid
    msg_buf->validMsg = 1;
    //this variable indicates where the actual command data is in the buffer
    unsigned int cmd_idx = 0;

    //check if this is an idle message
    if(data[0] == 0xff)
    {
        msg_buf->msg_type = dcc_msg_idle;
        return;
    }


    /*
    
    Addressing
    
    */
    //if MSb is 0, this address is a single-byte address for 7-bit decoders
    else if(data[0]>>7 == 0)
    {
        msg_buf->addr = data[0];
        cmd_idx = 1;
    }
    //if msb is 1 but next bit is 0, it is a single byte address too.
    else if(data[0]>>6 == 0x10)
    {
        msg_buf->addr = data[0];
        cmd_idx = 1;
    }
    //else there are two address bytes for a 14-bit address. This is always indicated with first two bits being 11, the following bits are the address bits.
    else
    {
        msg_buf->addr = ((data[0] & 0x3f) << 8) + data[1];
        cmd_idx = 2;
    }

    /*
    
    Command interpretation

    The first byte follows format of
    CCC DDDDD where first 3 bits are the command type and last 5 bytes are command arguments
    
    */
    // copy for easy debugging
    memcpy(&msg_buf->cmd_arg, &data[cmd_idx], MAX_BYTESIZE_CMD_ARG*sizeof(unsigned char));

    //first derrive the instruction type
    unsigned char instr = data[cmd_idx] >> 5;
    msg_buf->cmd = instr;
    unsigned char instr_arg = data[cmd_idx] & 0x1f;
    if(instr == 0b000)
        msg_buf->msg_type = dcc_msg_dcci;
    if(instr == 0b001)
        msg_buf->msg_type = dcc_msg_aoi;
    if(instr == 0b010 || instr == 0b011)
        msg_buf->msg_type = dcc_msg_sdi;
    if(instr == 0b100)
        msg_buf->msg_type = dcc_msg_fgi1;
    if(instr == 0b101)
        msg_buf->msg_type = dcc_msg_fgi2;
    if(instr == 0b110)
        msg_buf->msg_type = dcc_msg_fexp;
    if(instr == 0b111)
        msg_buf->msg_type = dcc_msg_cvai;

    /*
    
    
        consist control and advanced commands
    
    
    */
   //mostly TBD
    if(msg_buf->msg_type == dcc_msg_aoi)
    {
        if(instr_arg == 0x1f)
        {
            // 128 speed step mode

            //interpret second byte. Last 7 bits are speed value
            unsigned char speed_val = data[cmd_idx+1] & 0x7f;
            if(speed_val == 0 || speed_val == 1)
                msg_buf->speed = 0;
            //MSb indicated direction. 1 is fwd, 0 is rev
            else if(data[cmd_idx+1] >> 7)
                msg_buf->speed = (speed_val-1);
            else
                msg_buf->speed = -1*(speed_val-1);
        }
    }

   /*
   
        speed control 14/28 step mode
   
   */
    if(msg_buf->msg_type == dcc_msg_sdi)
    {
        //determine if 14 or 28 step mode?
        //Should actually read content of CV. Assume 28 step mode.
        if(DCC_14_MODE)
        {
            //14 step mode, with front light control in bit C.
            //format:
            //Reverse Operation   010 CDDDD
            //Forward Operation   011 CDDDD 
            unsigned char speed_val = instr_arg & 0x0f;
            if(speed_val == 0 || speed_val == 1)
                msg_buf->speed = 0;
            else if(instr&1)
                msg_buf->speed = speed_val-1;
            else
                msg_buf->speed = -1*(speed_val-1);

        }
        else
        {
            //28 step mode
            //format:
            //Reverse Operation   010 LSB MSB MSB-1 MSB-2 MSB-3
            //Forward Operation   011 LSB MSB MSB-1 MSB-2 MSB-3
            instr_arg &= 0x1f;
            msg_buf->speed = 0;
            unsigned char speed_lsb         = (instr_arg & 0b10000)>>4;
            unsigned char speed_msb0123     = (instr_arg & 0b01111)<<1;
            unsigned char speed_val = speed_msb0123 | speed_lsb;

            if(speed_val == 0 || speed_val == 2)
                msg_buf->speed = 0;
            else if(instr&1)
                msg_buf->speed = speed_val-3;
            else
               msg_buf->speed = -1*(speed_val-3);

        }        
    }

}