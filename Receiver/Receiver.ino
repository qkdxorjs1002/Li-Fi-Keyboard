#define _CDS_PIN A0

void keyboard_scan(uint8_t key);
void cds_scan();
int8_t decode_bits();

unsigned long static_avg = 0;
unsigned long dyn_avg = 0;
unsigned long dyn_avg_cnt = 0;

const bool bit[8][8] = {
    {0, 1, 0, 1, 1, 0, 1, 0}, // F1
    {0, 1, 1, 1, 1, 1, 1, 0}, // F2
    {1, 0, 1, 0, 0, 1, 0, 1}, // F3
    {1, 1, 0, 0, 0, 0, 1, 1}, // F4
    {0, 0, 1, 1, 1, 1, 0, 0}, // F5
    {0, 1, 1, 0, 0, 1, 1, 0}, // F6
    {1, 0, 0, 1, 1, 0, 0, 1}, // F7
    {1, 0, 1, 1, 1, 1, 0, 1}, // F8
};

#define _BIT_STREAM_SIZE 11
struct _bit_stream {
    bool queue[_BIT_STREAM_SIZE];
    int8_t queue_front;
    int8_t queue_rear;
    int8_t queue_size;

    _bit_stream() {
        queue[_BIT_STREAM_SIZE] = {{0, }, };
        queue_front = 0;
        queue_rear = 0;
        queue_size = 0;
    }
  
    void queue_push(bool _status) {
        if((queue_rear + 1) % _BIT_STREAM_SIZE == queue_front) {
            queue_pop();
        }
        queue_size++;
        queue_rear = (queue_rear + 1) % _BIT_STREAM_SIZE;
        queue[queue_rear] = _status;
    }

    void queue_pop() {
        if(queue_rear == queue_front) {
            return 0;
        }
        queue_front = (queue_front + 1) % _BIT_STREAM_SIZE;
        queue[queue_front] = 0;
        queue_size--;
    }
} bit_stream;

#define _BIT_TPHASE_SIZE 3
struct _bit_tphase {
    unsigned long queue[_BIT_TPHASE_SIZE][2];
    int8_t queue_front;
    int8_t queue_rear;
    int8_t queue_size;

    _bit_tphase() {
        queue[_BIT_TPHASE_SIZE][2] = {{0, }, };
        queue_front = 0;
        queue_rear = 0;
        queue_size = 0;
    }
  
    void queue_push(unsigned long _time, uint16_t _value) {
        if((queue_rear + 1) % _BIT_TPHASE_SIZE == queue_front) {
            queue_pop();
        }
        queue_size++;
        queue_rear = (queue_rear + 1) % _BIT_TPHASE_SIZE;
        queue[queue_rear][0] = _time;
        queue[queue_rear][1] = _value;
    }

    void queue_pop() {
        if(queue_rear == queue_front) {
            return ;
        }
        queue_front = (queue_front + 1) % _BIT_TPHASE_SIZE;
        queue[queue_front][0] = 0;
        queue[queue_front][1] = 0;
        queue_size--;
    }
} bit_tphase;

#define _BIT_WIDTH 1000
void setup() {
    Serial.begin(115200);
    pinMode(_CDS_PIN, INPUT);
    
    uint16_t static_avg_cnt = 0;
    while(millis() <= _BIT_WIDTH) {
        uint16_t value = analogRead(_CDS_PIN);
        static_avg += value;
        static_avg_cnt++;
    }
    static_avg /= static_avg_cnt;
    static_avg += 20;
}

void loop() {
    cds_scan();
    int8_t key = decode_bits();
    if(key != -1) {
        Serial.print("F");
        Serial.print(key + 1);
        Serial.println(" is pressed.");
    }
}

unsigned long pre_time = 0;
void cds_scan() {
    while(1) {
        unsigned long cur_time = micros();
        uint16_t value = analogRead(_CDS_PIN);
        if(cur_time - pre_time >= 5000) {
            bit_tphase.queue_push(cur_time, value);
            pre_time = cur_time;
            break;
        }
    }
}

#define _PULSE_WIDTH_ALLOWANCE 27500
unsigned long cnt_time = 0;
int8_t decode_bits() {
    unsigned long pre_bit[2] = {bit_tphase.queue[(bit_tphase.queue_front + 1) % _BIT_TPHASE_SIZE][0], bit_tphase.queue[(bit_tphase.queue_front + 1) % _BIT_TPHASE_SIZE][1]};
    unsigned long cur_bit[2] = {bit_tphase.queue[bit_tphase.queue_rear][0], bit_tphase.queue[bit_tphase.queue_rear][1]};
    unsigned long avg = 0;

    if(dyn_avg_cnt >= 100) {
        avg = (dyn_avg / dyn_avg_cnt) / 2;
    } else {
        avg = static_avg;
    }

    bool pre_bit_status = (pre_bit[1] >= avg);
    bool cur_bit_status = (cur_bit[1] >= avg);

    if(cur_bit_status) {
        dyn_avg += cur_bit[1];
        dyn_avg_cnt++;
    }

    if(pre_bit_status >= cur_bit_status) {
        cnt_time += cur_bit[0] - pre_bit[0];
        
        if(cnt_time >= _PULSE_WIDTH_ALLOWANCE) {
            bit_stream.queue_push(cur_bit_status);
            cnt_time = 0;
        }
    }

    bool bit_buffer[10] = {bit_stream.queue[(bit_stream.queue_front + 1) % _BIT_STREAM_SIZE], 0, bit_stream.queue[bit_stream.queue_rear]};
    for(uint8_t idx = 0; idx < _BIT_STREAM_SIZE - 1; idx++) {
        bit_buffer[idx] = bit_stream.queue[(bit_stream.queue_front + idx + 1) % _BIT_STREAM_SIZE];
    }

    for(uint8_t idx = 0; idx < 8; idx++) {
        bool parity = (bit_buffer[0] == bit_buffer[9]);
        for(uint8_t jdx = 0; jdx < 8; jdx++) {
            parity &= (bit_buffer[jdx + 1] == bit[idx][jdx]);
        }
        if(parity) {
            bit_stream.queue_pop();
            return idx;
        }
    }

    return -1;
}
