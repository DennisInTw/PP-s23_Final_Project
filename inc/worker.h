#include "intra.h"
#include "frame.h"
#include "macroblock.h"
#include "block.h"
#include "io.h"

// 根據實驗結果, MAX_THREADS = 2會有最好的效益
#define MAX_THREADS 4
#define EN_DBG_ENC_I_FRAME
//#define EN_DBG_ENC_Y_INTRA_16x16
//#define EN_DBG_ENC_Y_INTRA_4x4
//#define EN_DBG_ENC_CBCR
//#define EN_DBG_INTRA_MODES_16x16
//#define EN_DBG_INTRA_MODES_4x4

//#define TEST1_THREAD_IN_Y_INTRA_16x16
//#define TEST2_THREAD_IN_Y_INTRA_4x4
//#define TEST3_THREAD_IN_ENCODE_Y_INTRA16x16_BLOCK  // 只有create一個new thread
//#define TEST4_THREAD_IN_ENCODE_Y_INTRA4x4_BLOCK    // 根據MAX_THREADS來create thread
#define TEST5_THREAD_IN_ENCODE_ONE_FRAME             // 根據MAX_THREADS來create thread

class WorkerArgs {
    public:
      int threadId;
      MacroBlock* mb;
      std::vector<MacroBlock>* decoded_blocks;
      Frame* frame;

      WorkerArgs() {};
      WorkerArgs(const int id, MacroBlock* mb, std::vector<MacroBlock>* decoded_blocks, Frame* frame) {
        this->threadId = id;
        this->mb = mb;
        //this->mb = frame.mbs[mb_no];//macroblock;
        //this->decoded_blocks.assign(decoded_blocks.begin(), decoded_blocks.end());
        this->decoded_blocks = decoded_blocks;
        this->frame = frame;
      };

};
#if 0
class Worker_intra16x16 {
    public:
      int threadId;
      int predict_mode_start;
      int predict_mode_end;
      Predictor predictor;
      Block16x16 block;

      Worker_intra16x16() {};
};
#endif

class Worker_encode_one_frame {
    public:
      int threadId;
      Frame* frame;

      Worker_encode_one_frame(const int id, Frame* frame)
      {
        this->threadId = id;
        this->frame = frame;
      };
};

class Worker_Y_intra4x4_modes {
    public:
      int threadId;
      int predict_mode_start;
      int predict_mode_end;
      Predictor* predictor;
      Block4x4* block;

      Worker_Y_intra4x4_modes() {};
};

class Worker_Y_intra16x16_modes {
    public:
      int threadId;
      int predict_mode_start;
      int predict_mode_end;
      Predictor* predictor;
      Block16x16* block;

      Worker_Y_intra16x16_modes() {};
};

class Worker_Y_intra16x16_encode_block {
    public:
      int threadId;
      MacroBlock* mb;
      std::vector<MacroBlock>* decoded_blocks;
      Frame* frame;

      Worker_Y_intra16x16_encode_block() {};
      Worker_Y_intra16x16_encode_block(const int id, MacroBlock* mb, std::vector<MacroBlock>* decoded_blocks, Frame* frame) {
        this->threadId = id;
        this->mb = mb;
        this->decoded_blocks = decoded_blocks;
        this->frame = frame;
      };


};

class Worker_Y_intra4x4_encode_block {
    public:
      int threadId;
      MacroBlock* mb;
      std::vector<MacroBlock>* decoded_blocks;
      Frame* frame;
      int position;
      int pos_len;

      Worker_Y_intra4x4_encode_block() {};
      Worker_Y_intra4x4_encode_block(const int id, MacroBlock* mb, std::vector<MacroBlock>* decoded_blocks, Frame* frame, int pos, int len) {
        this->threadId = id;
        this->mb = mb;
        this->decoded_blocks = decoded_blocks;
        this->frame = frame;
        this->position = pos;
        this->pos_len = len;
      };


};

