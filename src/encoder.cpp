#include <iostream>
#include <thread>
#include "log.h"
#include "util.h"
#include "io.h"
#include "frame.h"
#include "frame_encode.h"
#include "frame_vlc.h"
#include <chrono>
#include "worker.h"

#define DBG_LOG

Log logger("Main");

void* operator new(std::size_t n) {
    // std::cerr << "[allocating " << n << " bytes]\n";
    return malloc(n);
}

void run_enc_vlc(Worker_encode_one_frame *const args)
{
//  printf("[DBG] th%d\n", args->threadId);

  #ifdef DBG_LOG
  auto begin_encode_I_frame = std::chrono::high_resolution_clock::now();
  #endif     
  encode_I_frame(*(args->frame));
  #ifdef DBG_LOG
  auto end_encode_I_frame = std::chrono::high_resolution_clock::now();
  auto dur_encode_I_frame = end_encode_I_frame - begin_encode_I_frame;
  auto us_encode_I_frame = std::chrono::duration_cast<std::chrono::microseconds>(dur_encode_I_frame).count();
  printf("[DBG] encode I-frame th%d start at %ld , finish at %ld , cost %ld us\n", args->threadId, begin_encode_I_frame, end_encode_I_frame, us_encode_I_frame);
  #endif

  #ifdef DBG_LOG
  auto begin_vlc = std::chrono::high_resolution_clock::now();
  #endif
  vlc_frame(*(args->frame));
  #ifdef DBG_LOG
  auto end_vlc = std::chrono::high_resolution_clock::now();
  auto dur_vlc = end_vlc - begin_vlc;
  auto us_vlc = std::chrono::duration_cast<std::chrono::microseconds>(dur_vlc).count();
  printf("[DBG] vlc th%d start at %ld , finish at %ld , cost %ld us\n", args->threadId, begin_vlc, end_vlc, us_vlc);
  #endif
}

void encode_sequence(Reader& reader, Writer& writer, Util& util) {
  int curr_frame = 0;

  writer.write_sps(util.width, util.height, reader.nb_frames);
  writer.write_pps();

  while (curr_frame < reader.nb_frames) {

#ifndef TEST5_THREAD_IN_ENCODE_ONE_FRAME
    #ifdef DBG_LOG
    auto begin_read_raw = std::chrono::high_resolution_clock::now();
    #endif

    // reader讀進raw data轉成YCbCr,然後frame會切成16x16的MB儲存在mbs
    Frame frame(reader.get_padded_frame());
    #ifdef DBG_LOG
    auto end_read_raw = std::chrono::high_resolution_clock::now();
    auto dur_read_raw = end_read_raw - begin_read_raw;
    auto us_read_raw = std::chrono::duration_cast<std::chrono::microseconds>(dur_read_raw).count();
    printf("[DBG] read raw and get YCbCr cost %ld us\n", us_read_raw);
    #endif

    logger.log(Level::NORMAL, "encode frame #" + std::to_string(curr_frame));
    if (util.test_frame != -1) {
      if (curr_frame == util.test_frame) {
        encode_I_frame(frame);
        break;
      }
    } else {
      #ifdef DBG_LOG
      auto begin_encode_I_frame = std::chrono::high_resolution_clock::now();
      #endif
      encode_I_frame(frame);
      #ifdef DBG_LOG
      auto end_encode_I_frame = std::chrono::high_resolution_clock::now();
      auto dur_encode_I_frame = end_encode_I_frame - begin_encode_I_frame;
      auto us_encode_I_frame = std::chrono::duration_cast<std::chrono::microseconds>(dur_encode_I_frame).count();
      printf("[DBG] encode I-frame cost %ld us\n", us_encode_I_frame);
      #endif

      #ifdef DBG_LOG
      auto begin_vlc = std::chrono::high_resolution_clock::now();
      #endif
      vlc_frame(frame);
      #ifdef DBG_LOG
      auto end_vlc = std::chrono::high_resolution_clock::now();
      auto dur_vlc = end_vlc - begin_vlc;
      auto us_vlc = std::chrono::duration_cast<std::chrono::microseconds>(dur_vlc).count();
      printf("[DBG] vlc cost %ld us\n", us_vlc);

      auto dur_whole_encode = end_vlc - begin_encode_I_frame;
      auto us_whole_encode = std::chrono::duration_cast<std::chrono::microseconds>(dur_whole_encode).count();
      printf("[DBG] whole encode cost %ld us\n", us_whole_encode);
      #endif
      writer.write_slice(curr_frame, frame);

      #ifdef DBG_LOG
      auto end_one_frame = std::chrono::high_resolution_clock::now();
      auto dur_one_frame = end_one_frame - begin_read_raw;
      auto us_one_frame = std::chrono::duration_cast<std::chrono::microseconds>(dur_one_frame).count();
      printf("[DBG] encode one frame cost %ld us\n", us_one_frame);

      auto dur_write_bitstream = end_one_frame - end_vlc;
      auto us_write_bitstream = std::chrono::duration_cast<std::chrono::microseconds>(dur_write_bitstream).count();
      printf("[DBG] write bitstream cost %ld us\n", us_write_bitstream);
      #endif
    }

    curr_frame++;
#else
#if (MAX_THREADS == 2)
    std::thread workers;

    #ifdef DBG_LOG
    auto begin_read_raw = std::chrono::high_resolution_clock::now();
    #endif
    // reader讀進raw data轉成YCbCr,然後frame會切成16x16的MB儲存在mbs
    Frame frame0(reader.get_padded_frame());
    logger.log(Level::NORMAL, "encode frame #" + std::to_string(curr_frame));

    if (reader.nb_frames - curr_frame >= 2) {
      // reader讀進raw data轉成YCbCr,然後frame會切成16x16的MB儲存在mbs
      Frame frame1(reader.get_padded_frame());
      #ifdef DBG_LOG
      auto end_read_raw = std::chrono::high_resolution_clock::now();
      auto dur_read_raw = end_read_raw - begin_read_raw;
      auto us_read_raw = std::chrono::duration_cast<std::chrono::microseconds>(dur_read_raw).count();
      printf("[DBG] read raw and get YCbCr for 2 threads cost %ld us\n", us_read_raw);
      #endif
 
      logger.log(Level::NORMAL, "encode frame #" + std::to_string(curr_frame+1));

      #ifdef DBG_LOG
      auto begin_encode_I_frame = std::chrono::high_resolution_clock::now();
      #endif
 
      Worker_encode_one_frame worker_one_frame1(1, &frame1);
      auto begin_th1_create = std::chrono::high_resolution_clock::now();
      printf("[DBG] create th1 start at %ld\n", begin_th1_create);
      // run thread 1
      workers = std::thread(run_enc_vlc, &worker_one_frame1);
      auto end_th1_create = std::chrono::high_resolution_clock::now();
      printf("[DBG] create th1 end at %ld\n", end_th1_create);
      
      encode_I_frame(frame0);
      #ifdef DBG_LOG
      auto end_encode_I_frame = std::chrono::high_resolution_clock::now();
      auto dur_encode_I_frame = end_encode_I_frame - begin_encode_I_frame;
      auto us_encode_I_frame = std::chrono::duration_cast<std::chrono::microseconds>(dur_encode_I_frame).count();
      printf("[DBG] encode I-frame main thread start at %ld , finish at %ld , cost %ld us\n",begin_encode_I_frame, end_encode_I_frame, us_encode_I_frame);
      #endif

      #ifdef DBG_LOG
      auto begin_vlc = std::chrono::high_resolution_clock::now();
      #endif
      vlc_frame(frame0);
      #ifdef DBG_LOG
      auto end_vlc = std::chrono::high_resolution_clock::now();
      auto dur_vlc = end_vlc - begin_vlc;
      auto us_vlc = std::chrono::duration_cast<std::chrono::microseconds>(dur_vlc).count();
      printf("[DBG] vlc main thread start at %ld , finish at %ld , cost %ld us\n", begin_vlc, end_vlc, us_vlc);


      #endif

      workers.join();
      auto th1_finish = std::chrono::high_resolution_clock::now();
      printf("[DBG] create th1 finsh at %ld\n", th1_finish);

      #ifdef DBG_LOG
      auto dur_whole_encode = th1_finish - begin_encode_I_frame;
      auto us_whole_encode = std::chrono::duration_cast<std::chrono::microseconds>(dur_whole_encode).count();
      printf("[DBG] whole encode cost %ld us\n", us_whole_encode);
      #endif

      writer.write_slice(curr_frame, frame0);
      writer.write_slice(curr_frame+1, frame1);
      #ifdef DBG_LOG
      auto end_one_frame = std::chrono::high_resolution_clock::now();
      auto dur_one_frame = end_one_frame - begin_read_raw;
      auto us_one_frame = std::chrono::duration_cast<std::chrono::microseconds>(dur_one_frame).count();
      printf("[DBG] encode one frame 2 threads cost %ld us\n", us_one_frame);

      auto dur_write_bitstream = end_one_frame - end_vlc;
      auto us_write_bitstream = std::chrono::duration_cast<std::chrono::microseconds>(dur_write_bitstream).count();
      printf("[DBG] write bitstream cost %ld us\n", us_write_bitstream);
 
     #endif


      curr_frame += 2;
    } else {
      encode_I_frame(frame0);
      vlc_frame(frame0);

      writer.write_slice(curr_frame, frame0);
      curr_frame++;
    }
#elif (MAX_THREADS == 3)
    std::thread workers[2];

    #ifdef DBG_LOG
    auto begin_read_raw = std::chrono::high_resolution_clock::now();
    #endif

    // reader讀進raw data轉成YCbCr,然後frame會切成16x16的MB儲存在mbs
    Frame frame0(reader.get_padded_frame());
    logger.log(Level::NORMAL, "encode frame #" + std::to_string(curr_frame));

    if (reader.nb_frames - curr_frame >= 3) {
      // reader讀進raw data轉成YCbCr,然後frame會切成16x16的MB儲存在mbs
      Frame frame1(reader.get_padded_frame());
      logger.log(Level::NORMAL, "encode frame #" + std::to_string(curr_frame+1));
      // reader讀進raw data轉成YCbCr,然後frame會切成16x16的MB儲存在mbs
      Frame frame2(reader.get_padded_frame());
      #ifdef DBG_LOG
      auto end_read_raw = std::chrono::high_resolution_clock::now();
      auto dur_read_raw = end_read_raw - begin_read_raw;
      auto us_read_raw = std::chrono::duration_cast<std::chrono::microseconds>(dur_read_raw).count();
      printf("[DBG] read raw and get YCbCr for 3 threads cost %ld us\n", us_read_raw);
      #endif

      logger.log(Level::NORMAL, "encode frame #" + std::to_string(curr_frame+2));

      #ifdef DBG_LOG
      auto begin_encode_I_frame = std::chrono::high_resolution_clock::now();
      #endif 
 
      Worker_encode_one_frame worker_one_frame1(1, &frame1);
      Worker_encode_one_frame worker_one_frame2(2, &frame2);
      auto begin_th1_create = std::chrono::high_resolution_clock::now();
      printf("[DBG] create th1/th2 start at %ld\n", begin_th1_create);

      // run thread 1
      workers[0] = std::thread(run_enc_vlc, &worker_one_frame1);
      // run thread 2
      workers[1] = std::thread(run_enc_vlc, &worker_one_frame2);
      auto end_th1_create = std::chrono::high_resolution_clock::now();
      printf("[DBG] create th1/th2 end at %ld\n", end_th1_create);

      encode_I_frame(frame0);
      #ifdef DBG_LOG
      auto end_encode_I_frame = std::chrono::high_resolution_clock::now();
      auto dur_encode_I_frame = end_encode_I_frame - begin_encode_I_frame;
      auto us_encode_I_frame = std::chrono::duration_cast<std::chrono::microseconds>(dur_encode_I_frame).count();
      printf("[DBG] encode I-frame main thread start at %ld , finish at %ld , cost %ld us\n",begin_encode_I_frame, end_encode_I_frame, us_encode_I_frame);
      #endif

      #ifdef DBG_LOG
      auto begin_vlc = std::chrono::high_resolution_clock::now();
      #endif
      vlc_frame(frame0);
      #ifdef DBG_LOG
      auto end_vlc = std::chrono::high_resolution_clock::now();
      auto dur_vlc = end_vlc - begin_vlc;
      auto us_vlc = std::chrono::duration_cast<std::chrono::microseconds>(dur_vlc).count();
      printf("[DBG] vlc main thread start at %ld , finish at %ld , cost %ld us\n", begin_vlc, end_vlc, us_vlc);
      #endif

      workers[0].join();
      workers[1].join();
      auto th1_finish = std::chrono::high_resolution_clock::now();
      printf("[DBG] create th1/th2 finsh at %ld\n", th1_finish);

      #ifdef DBG_LOG
      auto dur_whole_encode = th1_finish - begin_encode_I_frame;
      auto us_whole_encode = std::chrono::duration_cast<std::chrono::microseconds>(dur_whole_encode).count();
      printf("[DBG] whole encode cost %ld us\n", us_whole_encode);
      #endif

      writer.write_slice(curr_frame, frame0);
      writer.write_slice(curr_frame+1, frame1);
      writer.write_slice(curr_frame+2, frame2);

      #ifdef DBG_LOG
      auto end_one_frame = std::chrono::high_resolution_clock::now();
      auto dur_one_frame = end_one_frame - begin_read_raw;
      auto us_one_frame = std::chrono::duration_cast<std::chrono::microseconds>(dur_one_frame).count();
      printf("[DBG] encode one frame 3 thread cost %ld us\n", us_one_frame);

      auto dur_write_bitstream = end_one_frame - end_vlc;
      auto us_write_bitstream = std::chrono::duration_cast<std::chrono::microseconds>(dur_write_bitstream).count();
      printf("[DBG] write bitstream cost %ld us\n", us_write_bitstream);
 
      #endif
      curr_frame += 3;

    } else if (reader.nb_frames - curr_frame >= 2) {
      // reader讀進raw data轉成YCbCr,然後frame會切成16x16的MB儲存在mbs
      Frame frame1(reader.get_padded_frame());
      logger.log(Level::NORMAL, "encode frame #" + std::to_string(curr_frame+1));

      Worker_encode_one_frame worker_one_frame1(1, &frame1);
      // run thread 1
      workers[0] = std::thread(run_enc_vlc, &worker_one_frame1);
      
      encode_I_frame(frame0);
      vlc_frame(frame0);

      workers[0].join();

      writer.write_slice(curr_frame, frame0);
      writer.write_slice(curr_frame+1, frame1);
      curr_frame += 2;
    } else {
      encode_I_frame(frame0);
      vlc_frame(frame0);

      writer.write_slice(curr_frame, frame0);
      curr_frame++;
    }

#elif (MAX_THREADS == 4)
    std::thread workers[3];

    #ifdef DBG_LOG
    auto begin_read_raw = std::chrono::high_resolution_clock::now();
    #endif
    // reader讀進raw data轉成YCbCr,然後frame會切成16x16的MB儲存在mbs
    Frame frame0(reader.get_padded_frame());
    logger.log(Level::NORMAL, "encode frame #" + std::to_string(curr_frame));

    if (reader.nb_frames - curr_frame >= 4) {
      // reader讀進raw data轉成YCbCr,然後frame會切成16x16的MB儲存在mbs
      Frame frame1(reader.get_padded_frame());
      logger.log(Level::NORMAL, "encode frame #" + std::to_string(curr_frame+1));
      // reader讀進raw data轉成YCbCr,然後frame會切成16x16的MB儲存在mbs
      Frame frame2(reader.get_padded_frame());
      logger.log(Level::NORMAL, "encode frame #" + std::to_string(curr_frame+2));
      // reader讀進raw data轉成YCbCr,然後frame會切成16x16的MB儲存在mbs
      Frame frame3(reader.get_padded_frame());
      #ifdef DBG_LOG
      auto end_read_raw = std::chrono::high_resolution_clock::now();
      auto dur_read_raw = end_read_raw - begin_read_raw;
      auto us_read_raw = std::chrono::duration_cast<std::chrono::microseconds>(dur_read_raw).count();
      printf("[DBG] read raw and get YCbCr for 4 threads cost %ld us\n", us_read_raw);
      #endif

      logger.log(Level::NORMAL, "encode frame #" + std::to_string(curr_frame+3));

      #ifdef DBG_LOG
      auto begin_encode_I_frame = std::chrono::high_resolution_clock::now();
      #endif 
 
      Worker_encode_one_frame worker_one_frame1(1, &frame1);
      Worker_encode_one_frame worker_one_frame2(2, &frame2);
      Worker_encode_one_frame worker_one_frame3(3, &frame3);
      auto begin_th1_create = std::chrono::high_resolution_clock::now();
      printf("[DBG] create th1/th2/th3 start at %ld\n", begin_th1_create);

      // run thread 1
      workers[0] = std::thread(run_enc_vlc, &worker_one_frame1);
      // run thread 2
      workers[1] = std::thread(run_enc_vlc, &worker_one_frame2);
      // run thread 3
      workers[2] = std::thread(run_enc_vlc, &worker_one_frame3);
      auto end_th1_create = std::chrono::high_resolution_clock::now();
      printf("[DBG] create th1/th2/th3 end at %ld\n", end_th1_create);

      encode_I_frame(frame0);
      #ifdef DBG_LOG
      auto end_encode_I_frame = std::chrono::high_resolution_clock::now();
      auto dur_encode_I_frame = end_encode_I_frame - begin_encode_I_frame;
      auto us_encode_I_frame = std::chrono::duration_cast<std::chrono::microseconds>(dur_encode_I_frame).count();
      printf("[DBG] encode I-frame main thread start at %ld , finish at %ld , cost %ld us\n",begin_encode_I_frame, end_encode_I_frame, us_encode_I_frame);
      #endif

      #ifdef DBG_LOG
      auto begin_vlc = std::chrono::high_resolution_clock::now();
      #endif
      vlc_frame(frame0);
      #ifdef DBG_LOG
      auto end_vlc = std::chrono::high_resolution_clock::now();
      auto dur_vlc = end_vlc - begin_vlc;
      auto us_vlc = std::chrono::duration_cast<std::chrono::microseconds>(dur_vlc).count();
      printf("[DBG] vlc main thread start at %ld , finish at %ld , cost %ld us\n", begin_vlc, end_vlc, us_vlc);
      #endif

      workers[0].join();
      workers[1].join();
      workers[2].join();
      auto th1_finish = std::chrono::high_resolution_clock::now();
      printf("[DBG] create th1 finsh at %ld\n", th1_finish);

      #ifdef DBG_LOG
      auto dur_whole_encode = th1_finish - begin_encode_I_frame;
      auto us_whole_encode = std::chrono::duration_cast<std::chrono::microseconds>(dur_whole_encode).count();
      printf("[DBG] whole encode cost %ld us\n", us_whole_encode);
      #endif

      writer.write_slice(curr_frame, frame0);
      writer.write_slice(curr_frame+1, frame1);
      writer.write_slice(curr_frame+2, frame2);
      writer.write_slice(curr_frame+3, frame3);
      #ifdef DBG_LOG
      auto end_one_frame = std::chrono::high_resolution_clock::now();
      auto dur_one_frame = end_one_frame - begin_read_raw;
      auto us_one_frame = std::chrono::duration_cast<std::chrono::microseconds>(dur_one_frame).count();
      printf("[DBG] encode one frame 4 threads cost %ld us\n", us_one_frame);

      auto dur_write_bitstream = end_one_frame - end_vlc;
      auto us_write_bitstream = std::chrono::duration_cast<std::chrono::microseconds>(dur_write_bitstream).count();
      printf("[DBG] write bitstream cost %ld us\n", us_write_bitstream);
 
      #endif

      curr_frame += 4;

    } else if (reader.nb_frames - curr_frame >= 3) {
      // reader讀進raw data轉成YCbCr,然後frame會切成16x16的MB儲存在mbs
      Frame frame1(reader.get_padded_frame());
      logger.log(Level::NORMAL, "encode frame #" + std::to_string(curr_frame+1));
      // reader讀進raw data轉成YCbCr,然後frame會切成16x16的MB儲存在mbs
      Frame frame2(reader.get_padded_frame());
      logger.log(Level::NORMAL, "encode frame #" + std::to_string(curr_frame+2));

      Worker_encode_one_frame worker_one_frame1(1, &frame1);
      Worker_encode_one_frame worker_one_frame2(2, &frame2);

      // run thread 1
      workers[0] = std::thread(run_enc_vlc, &worker_one_frame1);
      // run thread 2
      workers[1] = std::thread(run_enc_vlc, &worker_one_frame2);
      
      encode_I_frame(frame0);
      vlc_frame(frame0);

      workers[0].join();
      workers[1].join();

      writer.write_slice(curr_frame, frame0);
      writer.write_slice(curr_frame+1, frame1);
      writer.write_slice(curr_frame+2, frame2);
      curr_frame += 3;

    } else if (reader.nb_frames - curr_frame >= 2) {
      // reader讀進raw data轉成YCbCr,然後frame會切成16x16的MB儲存在mbs
      Frame frame1(reader.get_padded_frame());
      logger.log(Level::NORMAL, "encode frame #" + std::to_string(curr_frame+1));

      Worker_encode_one_frame worker_one_frame1(1, &frame1);
      // run thread 1
      workers[0] = std::thread(run_enc_vlc, &worker_one_frame1);
      
      encode_I_frame(frame0);
      vlc_frame(frame0);

      workers[0].join();

      writer.write_slice(curr_frame, frame0);
      writer.write_slice(curr_frame+1, frame1);
      curr_frame += 2;
    } else {
      encode_I_frame(frame0);
      vlc_frame(frame0);

      writer.write_slice(curr_frame, frame0);
      curr_frame++;
    }



#endif    
#endif
  }
}

int main(int argc, const char *argv[]) {
  // Get command-line arguments
  Util util(argc, argv);

  // Read from given filename
  Reader reader(util.input_file, util.width, util.height);

  // Write to given filename
  Writer writer(util.output_file);

  // Encoding process start
  encode_sequence(reader, writer, util);

  return EXIT_SUCCESS;
}
