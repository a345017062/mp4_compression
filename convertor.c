#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/avutil.h"
#include "libavutil/opt.h"
#include "libavutil/pixdesc.h"
#include "libavutil/mathematics.h"

static AVFormatContext *ifmt_ctx;
static AVFormatContext *ofmt_ctx;

static int open_input_file(const char *filename)
{
    int ret;
    unsigned int i;
    ifmt_ctx = NULL;
    AVDictionary* optionsDict = NULL;
    
    AVInputFormat *file_iformat = NULL;
    
    if (!(file_iformat = av_find_input_format("mp4"))) {
        av_log(NULL, AV_LOG_FATAL, "Unknown input format: 'mp4'\n");
        return -1;
    }
    
    av_dict_set_int(&optionsDict, "export_all", 0, 0);//设置demuxer的option，会返回不支持的key-value对。可以参照mov.c
    if ((ret = avformat_open_input(&ifmt_ctx, filename, NULL, &optionsDict)) < 0) {
        av_dict_free(&optionsDict);
        av_log(NULL, AV_LOG_ERROR, "Cannot open input file\n");
        return ret;
    }
    av_dict_free(&optionsDict);
    
    if ((ret = avformat_find_stream_info(ifmt_ctx, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
        return ret;
    }
    for (i = 0; i < ifmt_ctx->nb_streams; i++) {
        AVStream *stream;
        AVCodecContext *codec_ctx;
        stream = ifmt_ctx->streams[i];
        codec_ctx = stream->codec;
        /* Reencode video & audio and remux subtitles etc. */
        if (codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO
            || codec_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
            /* Open decoder */
            ret = avcodec_open2(codec_ctx,
                                avcodec_find_decoder(codec_ctx->codec_id), NULL);
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "Failed to open decoder for stream #%u\n", i);
                return ret;
            }
        }
    }
    av_dump_format(ifmt_ctx, 0, filename, 0);
    return 0;
}
static int open_output_file(const char *filename)
{
    AVStream *out_stream;
    AVStream *in_stream;
    AVCodecContext *dec_ctx, *enc_ctx;
    AVCodec *encoder;
    int ret;
    unsigned int i;
    AVDictionary* optionsDict = NULL;
    
    ofmt_ctx = NULL;
    avformat_alloc_output_context2(&ofmt_ctx, NULL, "mp4", filename);
    if (!ofmt_ctx) {
        av_log(NULL, AV_LOG_ERROR, "Could not create output context\n");
        return AVERROR_UNKNOWN;
    }
    
    if (av_opt_find(ofmt_ctx,"flush_packets",NULL,0,0)) {
        ret = av_opt_set_int(ofmt_ctx, "flush_packets", 1, 0);//设置流程中规定，最大值为1.
        if (ret < 0) {
            
        }
    }
    
    for (i = 0; i < ifmt_ctx->nb_streams; i++) {
        out_stream = avformat_new_stream(ofmt_ctx, NULL);
        if (!out_stream) {
            av_log(NULL, AV_LOG_ERROR, "Failed allocating output stream\n");
            return AVERROR_UNKNOWN;
        }
        in_stream = ifmt_ctx->streams[i];
        dec_ctx = in_stream->codec;
        
        if (dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO){
            encoder = avcodec_find_encoder_by_name("libx264");
            enc_ctx = avcodec_alloc_context3(encoder);
            out_stream->codec = enc_ctx;
            
            enc_ctx->height = dec_ctx->height;
            enc_ctx->width = dec_ctx->width;
            enc_ctx->sample_aspect_ratio = dec_ctx->sample_aspect_ratio;
            /* take first format from list of supported formats */
            enc_ctx->pix_fmt = encoder->pix_fmts[0];
            /* video time_base can be set to whatever is handy and supported by encoder */
            enc_ctx->time_base = dec_ctx->time_base;
            
            //这段代码要放到avcodec_open2前面，否则finder中无法解析出视频缩略图。
            if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER){//AV_CODEC_FLAG_LOOP_FILTER,AV_CODEC_FLAG_PASS1,AV_CODEC_FLAG_PASS2,AV_CODEC_FLAG_PSNR,AV_CODEC_FLAG_INTERLACED_DCT,AV_CODEC_FLAG_CLOSED_GOP,
                enc_ctx->flags |= CODEC_FLAG_GLOBAL_HEADER;
            }
            
            AVDictionary* optionsDict = NULL;
            av_dict_set_int(&optionsDict, "crf", 26, 0);//默认取值23，范围0~51，该值越大，压缩后视频的质量越差，速度越快。
//            av_dict_set_int(&optionsDict, "fastfirstpass", 1, 0);//默认就是1
            av_dict_set(&optionsDict, "level", "31", 0);//取值不要高于源的level，否则无意义。有关level，请参考这篇文章：http://www.cnblogs.com/zyl910/archive/2011/12/08/h264_level.html
            //下面几个x264内部参数，调整一下可以改变压缩后视频的编码速度、码率，有些参数对码率影响明显。但整体对编码速度的影响不明显。
//            av_dict_set(&optionsDict, "profile", "main", 0);//baseline,high,high10,high422,high444,main
//            av_dict_set(&optionsDict, "tune", "film", 0);//film,animation,grain,stillimage,psnr,ssim,fastdecode,zerolantency
//            av_dict_set(&optionsDict, "preset", "fast", 0);//ultrafast,superfast,veryfast,faster,fast,medium,slow,slower,veryslow,placebo
            ret = avcodec_open2(enc_ctx, encoder, &optionsDict);
            av_dict_free(&optionsDict);
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "Cannot open video encoder for stream #%u\n", i);
                return ret;
            }
        }else if (dec_ctx->codec_type == AVMEDIA_TYPE_AUDIO){
            encoder = avcodec_find_encoder_by_name("libfaac");
            enc_ctx = avcodec_alloc_context3(encoder);
            out_stream->codec = enc_ctx;
            
            enc_ctx->sample_rate = dec_ctx->sample_rate;
            enc_ctx->channel_layout = dec_ctx->channel_layout;
            enc_ctx->channels = av_get_channel_layout_nb_channels(enc_ctx->channel_layout);
            /* take first format from list of supported formats */
            enc_ctx->sample_fmt = encoder->sample_fmts[0];
            AVRational time_base={1, enc_ctx->sample_rate};
            enc_ctx->time_base = time_base;
            
            //这段代码要放到avcodec_open2前面，否则无法使用libfaac进行encode。
            if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER){
                enc_ctx->flags |= CODEC_FLAG_GLOBAL_HEADER;
            }
            
            ret = avcodec_open2(enc_ctx, encoder, NULL);
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "Cannot open audio encoder for stream #%u\n", i);
                return ret;
            }
        }else{
            av_log(NULL, AV_LOG_ERROR, "unkown codec\n");
            return -1;
        }
    }
    av_dump_format(ofmt_ctx, 0, filename, 1);
    if (!(ofmt_ctx->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&ofmt_ctx->pb, filename, AVIO_FLAG_WRITE);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Could not open output file '%s'", filename);
            return ret;
        }
    }
    
    av_dict_set_int(&optionsDict, "use_editlist", 0, 0);//设置muxer的option，会返回不支持的key-value对。可以参照movenc.c
    /* init muxer, write output file header */
    ret = avformat_write_header(ofmt_ctx, &optionsDict);
    av_dict_free(&optionsDict);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Error occurred when opening output file\n");
        return ret;
    }
    return 0;
}
static int encode_write_frame(AVFrame *filt_frame, unsigned int stream_index, int *got_frame) {
    int ret;
    int got_frame_local;
    AVPacket enc_pkt;
    int (*enc_func)(AVCodecContext *, AVPacket *, const AVFrame *, int *) = (ifmt_ctx->streams[stream_index]->codec->codec_type == AVMEDIA_TYPE_VIDEO) ? avcodec_encode_video2 : avcodec_encode_audio2;
    if (!got_frame)
        got_frame = &got_frame_local;
//    av_log(NULL, AV_LOG_INFO, "Encoding frame\n");
    /* encode filtered frame */
    enc_pkt.data = NULL;
    enc_pkt.size = 0;
    av_init_packet(&enc_pkt);
    ret = enc_func(ofmt_ctx->streams[stream_index]->codec, &enc_pkt,
                   filt_frame, got_frame);
    if (ret < 0)
        return ret;
    if (!(*got_frame))
        return 0;
    /* prepare packet for muxing */
    enc_pkt.stream_index = stream_index;
    enc_pkt.dts = av_rescale_q_rnd(enc_pkt.dts,
                                   ofmt_ctx->streams[stream_index]->codec->time_base,
                                   ofmt_ctx->streams[stream_index]->time_base,
                                   (enum AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
    enc_pkt.pts = av_rescale_q_rnd(enc_pkt.pts,
                                   ofmt_ctx->streams[stream_index]->codec->time_base,
                                   ofmt_ctx->streams[stream_index]->time_base,
                                   (enum AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
    enc_pkt.duration = av_rescale_q(enc_pkt.duration,
                                    ofmt_ctx->streams[stream_index]->codec->time_base,
                                    ofmt_ctx->streams[stream_index]->time_base);
//    av_log(NULL, AV_LOG_DEBUG, "Muxing frame\n");
    /* mux encoded frame */
    ret = av_interleaved_write_frame(ofmt_ctx, &enc_pkt);
    return ret;
}

static int flush_encoder(unsigned int stream_index)
{
    int ret;
    int got_frame;
    if (!(ofmt_ctx->streams[stream_index]->codec->codec->capabilities &
          CODEC_CAP_DELAY))
        return 0;
    while (1) {
//        av_log(NULL, AV_LOG_INFO, "Flushing stream #%u encoder\n", stream_index);
        ret = encode_write_frame(NULL, stream_index, &got_frame);
        if (ret < 0)
            break;
        if (!got_frame)
            return 0;
    }
    return ret;
}

int ynnb_fast_convertor(const char* inputPath, const char* outputPath)
{
    int ret;
    AVPacket packet;
    AVFrame *frame = NULL;
    enum AVMediaType type;
    unsigned int stream_index;
    unsigned int i;
    int got_frame;
    int (*dec_func)(AVCodecContext *, AVFrame *, int *, const AVPacket *);
    
    av_register_all();
    if ((ret = open_input_file(inputPath)) < 0)
        goto end;
    if ((ret = open_output_file(outputPath)) < 0)
        goto end;
    
    frame = av_frame_alloc();
    if (!frame) {
        ret = AVERROR(ENOMEM);
        goto end;
    }
    /* read all packets */
    while (1) {
        if ((ret = av_read_frame(ifmt_ctx, &packet)) < 0)
            break;
        stream_index = packet.stream_index;
        type = ifmt_ctx->streams[packet.stream_index]->codec->codec_type;
//        av_log(NULL, AV_LOG_DEBUG, "Demuxer gave frame of stream_index %u\n",
//               stream_index);
//        av_log(NULL, AV_LOG_DEBUG, "Going to reencode&filter the frame\n");
        
        
        packet.dts = av_rescale_q_rnd(packet.dts, ifmt_ctx->streams[stream_index]->time_base, ifmt_ctx->streams[stream_index]->codec->time_base, (enum AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
        packet.pts = av_rescale_q_rnd(packet.pts, ifmt_ctx->streams[stream_index]->time_base, ifmt_ctx->streams[stream_index]->codec->time_base, (enum AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
        dec_func = (type == AVMEDIA_TYPE_VIDEO) ? avcodec_decode_video2 :
        avcodec_decode_audio4;
        ret = dec_func(ifmt_ctx->streams[stream_index]->codec, frame,
                       &got_frame, &packet);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Decoding failed\n");
            break;
        }
        if (got_frame) {
            frame->pts = av_frame_get_best_effort_timestamp(frame);
            ret = encode_write_frame(frame, stream_index,NULL);
            if (ret < 0){
                goto end;
            }
        }
        av_packet_unref(&packet);
    }
    /* flush filters and encoders */
    for (i = 0; i < ifmt_ctx->nb_streams; i++) {
        /* flush encoder */
        ret = flush_encoder(i);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Flushing encoder failed\n");
            goto end;
        }
    }
    av_write_trailer(ofmt_ctx);
end:
    av_packet_unref(&packet);
    av_frame_free(&frame);
    for (i = 0; i < ifmt_ctx->nb_streams; i++) {
        avcodec_close(ifmt_ctx->streams[i]->codec);
        if (ofmt_ctx && ofmt_ctx->nb_streams > i && ofmt_ctx->streams[i] && ofmt_ctx->streams[i]->codec)
            avcodec_close(ofmt_ctx->streams[i]->codec);
    }
    avformat_close_input(&ifmt_ctx);
    if (ofmt_ctx && !(ofmt_ctx->oformat->flags & AVFMT_NOFILE))
        avio_close(ofmt_ctx->pb);
    avformat_free_context(ofmt_ctx);
    if (ret < 0)
        av_log(NULL, AV_LOG_ERROR, "Error occurred\n");
    return (ret? 1:0);
}
