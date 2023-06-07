# H.264 Intra-coding Encoder

NYCU PP-s23 Final Project

### Reference source code
We modify below source code and use parallel way to speed-up encoding.
https://github.com/yistLin/H264-Encoder

### Prepare input raw data
To generate raw data :
```bash
ffmpeg -i input_file.mp4 -vcodec rawvideo -pix_fmt rgb24 input_file.rgb
```

To confirm raw data :
```
ffplay -f rawvideo -pixel_format rgb24 -video_size 202x360 input_file.raw
```

### How to make and run program
```bash
make 
```

* input_file_size = input_file_width * input_file_height
* h264 file will be generated in video folder.

```bash
./encoder -v true -d true -size input_file_size -input video/input_file.rgb -output video/input_file.264
```

