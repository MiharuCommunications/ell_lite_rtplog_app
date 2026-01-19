#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <time.h>
#include <stdint.h>
#include <math.h>

#ifndef TZ_LOCAL
    #define TZ_LOCAL "Asia/Tokyo"
#endif

#define DSIZE 18
#define FMT_LOGTIME "%Y/%m/%d %H:%M:%S"
#define FMT_CSV "Time,Category,Value,Unit,SeqNum,SeqNumPrev,SendTimeStampDiff(ms),RecvTimeStampDiff(ms)\n"
#define FMT_CSV_SUMMARY "Time,Category,Value,Unit\n"
//#define FMT_CSV_SUMMARY "Time,JitterMoment(ms),JitterAverage(ms),PacketLoss(pkt/sec),Duplicate(pkt/sec),Reordering(pkt/sec),ReorderingLength(pkt)\n"

#ifdef DEBUG
    #define dbg_printf(...) printf(__VA_ARGS__)
#else
    #define dbg_printf(...) do{} while(0)
#endif

#define csvdump(category, val, unit) \
    fprintf(csv_fp, "%s.%d,%s,%d,%s,%d,%d,%.3f,%.3f\n", logtime, ms, category, val, unit, rtp_sn, rtp_sn_prev, snd_ts_diff_ms, rcv_ts_diff_ms)
    //fprintf(csv_fp, "%s,%s,%d,%s,%d,%d,%.3f,%.3f\n", logtime, category, val, unit, rtp_sn, rtp_sn_prev, snd_ts_diff_ms, rcv_ts_diff_ms)

typedef struct __attribute__((__packed__)) {
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
    double dms;
    double jms;
    int loss;
    int loss_burst;
    int duplicate;
    int reorder_count;
    int reorder_length;
} RTPStat;

int scanExtract(const struct dirent*  dir)
{
    return (dir->d_name[0] != '.');
} 

// ファイル名から struct tm を取得 (UTC)
struct tm parse_filename_to_tm(const char *filename) {
    struct tm tm_info = {0};
    sscanf(filename, "%4d%2d%2d_%2d%2d",
           &tm_info.tm_year, &tm_info.tm_mon, &tm_info.tm_mday,
           &tm_info.tm_hour, &tm_info.tm_min);
    tm_info.tm_year -= 1900;
    tm_info.tm_mon -= 1;
    tm_info.tm_sec = 0;
    return tm_info;
}

int main(int argc, char *argv[]) {
    char *log_dir = (argc == 1) ? "rtp-log" : argv[1];
    uint32_t plot_rate = (argc <= 2) ? 1 : atoi(argv[2]);
    DIR *dir;
    char logtime[100] = "0";
    struct dirent **entry;
    uint64_t packet_count = 0;
    uint16_t rtp_sn_prev = 0, rtp_sn = 0;
    double snd_ts_prev = 0, snd_ts = 0, snd_ts_diff = 0, snd_ts_diff_ms = 0;
    double rcv_ts_prev = 0, rcv_ts = 0, rcv_ts_diff = 0, rcv_ts_diff_ms = 0;
    double d = 0, j = 0, d_prev = 0, j_prev = 0;
    double d_max = 0, j_max = 0;
    int loss = 0;
    int loss_burst_max = 0;
    int reorder = 0;
    int reorder_count = 0;
    int reorder_length = 0;
    int reorder_length_max = 0;
    int duplicate = 0;

    if(plot_rate == 0 || plot_rate > 1000) {
        fprintf(stderr, "Invalid DumpRate : %s (range 1 - 1000)\n", argv[2]);
        return 1;
    }
    plot_rate = 1000 / plot_rate;

    dir = opendir(log_dir);
    if (!dir) {
        fprintf(stderr, "Invalid LogFileDirectory : %s\n", log_dir);
        return 1;
    }

    FILE *dump_fp = fopen("rtp_dump.bin", "wb");
    if (!dump_fp) {
        perror("fopen");
        return 1;
    }

    FILE *csv_fp = fopen("rtp_dump_detail.csv", "w");
    if (!csv_fp) {
        perror("fopen");
        return 1;
    }
    fprintf(csv_fp, FMT_CSV);

    FILE *csv_fp_summary = fopen("rtp_dump_summary.csv", "w");
    if (!csv_fp) {
        perror("fopen");
        return 1;
    }
    fprintf(csv_fp_summary, FMT_CSV_SUMMARY);

    int file_count = scandir(log_dir, &entry, scanExtract, alphasort);

    for (int i = 0; i < file_count; i++) {
        if (strstr(entry[i]->d_name, ".bin")) {
            char filepath[512];
            snprintf(filepath, sizeof(filepath), "%s/%s", log_dir, entry[i]->d_name);
            //printf("Processing %s\n", filepath);

            FILE *fp = fopen(filepath, "rb");
            if (!fp) continue;

            fseek(fp, 0, SEEK_END);
            long file_size = ftell(fp);
            fseek(fp, 0, SEEK_SET);

            uint8_t *data = malloc(file_size);
            fread(data, 1, file_size, fp);
            fclose(fp);

            uint32_t ofs = 0;
            char base_name[256];
            strncpy(base_name, entry[i]->d_name, sizeof(base_name));
            base_name[strlen(base_name) - 4] = '\0'; // ".bin"削除
            struct tm tm_info = parse_filename_to_tm(base_name);

            // mktimeでUTCのtime_tを取得
            setenv("TZ", "UTC", 1);
            tzset();
            time_t time_utc = mktime(&tm_info);                              

            // PCのタイムゾーンに変換
            setenv("TZ", TZ_LOCAL, 1);
            tzset();
            struct tm *tm_log = localtime(&time_utc);
            strftime(logtime, sizeof(logtime), FMT_LOGTIME, tm_log);

            int ms = 0;
            char is_start = 0;

            while (ofs + DSIZE <= file_size) {
                if(reorder == 0) {
                    rtp_sn_prev = rtp_sn;
                    snd_ts_prev = snd_ts;
                    rcv_ts_prev = rcv_ts;
                }
                reorder = 0;
                memcpy(&rtp_sn, data + ofs, 2);

                uint32_t snd_sec, snd_nsec, rcv_sec, rcv_nsec;
                memcpy(&snd_sec, data + ofs + 2, 4);
                memcpy(&snd_nsec, data + ofs + 6, 4);
                memcpy(&rcv_sec, data + ofs + 10, 4);
                memcpy(&rcv_nsec, data + ofs + 14, 4);

                snd_ts = snd_sec + snd_nsec * 1e-9;
                rcv_ts = rcv_sec + rcv_nsec * 1e-9;

                snd_ts_diff = snd_ts - snd_ts_prev;
                rcv_ts_diff = rcv_ts - rcv_ts_prev;
                snd_ts_diff_ms = snd_ts_diff * 1000;
                rcv_ts_diff_ms = rcv_ts_diff * 1000;

                ofs += DSIZE;

                if (packet_count > 1000) {
                    if (rtp_sn == rtp_sn_prev) {
                        // Duplicate Packet
                        duplicate++;
                        dbg_printf("[%s]:Duplicate (%d,%d) snd_ts(%f, %f)\n", logtime, rtp_sn, rtp_sn_prev, snd_ts, snd_ts_prev);
                        csvdump("Duplicate", 1, "Packets");
                    }
                    else if ((rtp_sn > 0 && rtp_sn - rtp_sn_prev != 1) ||
                             (rtp_sn == 0 && rtp_sn_prev != 0xFFFF)) {
                        if(snd_ts < snd_ts_prev) {
                            // Reordering
                            reorder = 1;
                            reorder_count++;
                            if(rtp_sn < rtp_sn_prev) {
                                reorder_length = rtp_sn_prev - rtp_sn;
                            }
                            else {
                                reorder_length = rtp_sn_prev - rtp_sn + 0x10000;
                            }

                            if(reorder_length_max < reorder_length) {
                                reorder_length_max = reorder_length;
                            }

                            dbg_printf("[%s]:Reordering %d(%d,%d) snd_ts(%f, %f)\n", logtime, reorder_length, rtp_sn, rtp_sn_prev, snd_ts, snd_ts_prev);
                            csvdump("Reordering", reorder_length, "Packets");
                        }
                        else {
                            // Packet Loss
                            int loss_length;
                            if(rtp_sn < rtp_sn_prev) {
                                loss_length = rtp_sn - rtp_sn_prev + 0x10000 - 1;
                            }
                            else {
                                loss_length = rtp_sn - rtp_sn_prev - 1;
                            }
                            loss += loss_length;

                            if(loss_burst_max < loss_length){
                                loss_burst_max = loss_length;
                            }

                            if(loss_length > 1) {
                                dbg_printf("[%s]:Burst Packet Loss:%d(%d,%d) \n", logtime, loss_length, rtp_sn, rtp_sn_prev);
                            }
                            else {
                                dbg_printf("[%s]:Packet Loss:%d(%d,%d) \n", logtime, loss_length, rtp_sn, rtp_sn_prev);
                            }
                            csvdump("Loss", loss_length, "Packets");
                        }
                    }
                    else {
                        // Jitter Calculation
                        if(snd_ts < snd_ts_prev) {
                            // FIXME ELL側アプリ修正が必要
                            // 送信カウンタ読み出し時に秒繰り上がりを検知する必要あり
                            // 暫定でPC側アプリで同等処理
                            if((uint32_t)snd_ts_prev == (uint32_t)snd_ts) snd_ts = snd_ts + 1.0;
                        }

                        // for DEBUG
                        if(rcv_ts < rcv_ts_prev) {
                            dbg_printf("(%d,%d)%ld:rcv:%f(%d,%d), %f\n", rtp_sn, rtp_sn_prev, packet_count, rcv_ts, rcv_sec, rcv_nsec, rcv_ts_prev);
                        }
                        if(snd_ts < snd_ts_prev) {
                            dbg_printf("(%d,%d)%ld:snd:%f(%d,%d), %f\n", rtp_sn, rtp_sn_prev, packet_count, snd_ts, snd_sec, snd_nsec, snd_ts_prev);
                        }

                        //rcv_ts_diff = (rcv_ts < rcv_ts_prev) ? (pow(2,32) + rcv_ts) - rcv_ts_prev : rcv_ts - rcv_ts_prev;
                        //snd_ts_diff = (snd_ts < snd_ts_prev) ? (pow(2,32) + snd_ts) - snd_ts_prev : snd_ts - snd_ts_prev;
                        rcv_ts_diff = rcv_ts - rcv_ts_prev;
                        snd_ts_diff = snd_ts - snd_ts_prev;
                        d = fabs(rcv_ts_diff - snd_ts_diff);
                        j = j_prev + (d_prev - j_prev) / 16;
                        if (d > d_max) d_max = d;
                        if (j > j_max) j_max = j;

                        if(d * 1000 > 100) {
                            dbg_printf("[%s]:Jitter:%f ms SN(%d,%d), TS(%f, %f):\n", logtime, d * 1000, rtp_sn, rtp_sn_prev, snd_ts, snd_ts_prev);
                            csvdump("Jitter", (int)(d*1000), "ms");
                        }
                    }
                }

                // Dump plotdata_bin & summary_csv
                if (packet_count % plot_rate == 0 && packet_count > 0) {
                    RTPStat stat;
                    stat.year = tm_log->tm_year + 1900;
                    stat.month = tm_log->tm_mon + 1;
                    stat.day = tm_log->tm_mday;
                    stat.hour = tm_log->tm_hour;
                    stat.minute = tm_log->tm_min;
                    stat.second = tm_log->tm_sec;
                    stat.dms = d_max * 1e3;
                    stat.jms = j_max * 1e3;
                    stat.loss = loss;
                    stat.loss_burst = loss_burst_max;
                    stat.duplicate = duplicate;
                    stat.reorder_count = reorder_count;
                    stat.reorder_length = reorder_length_max;

                    fwrite(&stat, sizeof(RTPStat), 1, dump_fp);

                    if(stat.dms > 100){
                        fprintf(csv_fp_summary, "%s,Jitter,%.3f,ms\n", logtime, stat.dms);
                    }

                    if(stat.loss > 0){
                        fprintf(csv_fp_summary, "%s,Loss(total),%d,pkt/sec\n", logtime, stat.loss);
                        fprintf(csv_fp_summary, "%s,Loss(burst),%d,pkt\n", logtime, stat.loss_burst);
                    }

                    if(stat.duplicate){
                        fprintf(csv_fp_summary, "%s,Duplicate,%d,pkt/sec\n", logtime, stat.duplicate);
                    }

                    if(stat.reorder_count > 0){
                        fprintf(csv_fp_summary, "%s,Reordering(count),%d,pkt/sec\n", logtime, stat.reorder_count);
                        fprintf(csv_fp_summary, "%s,Reordering(length),%d,pkt/sec\n", logtime, stat.reorder_length);
                    }
                    //fprintf(csv_fp_summary, "%s,%.3f,%.3f,%d,%d,%d,%d\n", logtime, stat.dms, stat.jms, stat.loss, stat.duplicate, stat.reorder_count, stat.reorder_length);

                    //if(stat.dms > 100)
                    //    printf("[Jitter] %d ms (%d%02d%02d_%02d:%02d:%02d)\n", (int)stat.dms, stat.year, stat.month, stat.day, stat.hour, stat.minute, stat.second);

                    //if(stat.loss > 20)
                    //    printf("[Loss] %d packets/sec (%d%02d%02d_%02d:%02d:%02d)\n", (int)stat.loss, stat.year, stat.month, stat.day, stat.hour, stat.minute, stat.second);

                    d_max = 0;
                    j_max = 0;
                    loss = 0;
                    loss_burst_max = 0;
                    duplicate = 0;
                    reorder_count = 0;
                    reorder_length_max = 0;
                }

                d_prev = d;
                j_prev = j;

                // 1ms加算
                if ( is_start )
                    ms++;

                if (ms >= 1000) {
                    ms = 0;
                    tm_log->tm_sec++;
                    mktime(tm_log); // 正規化（秒→分→時→日→月→年）
                    strftime(logtime, sizeof(logtime), FMT_LOGTIME, tm_log);
                }

                packet_count++;
                is_start = 1;
            }

            free(data);
        }
        free(entry[i]);
        //printf("%ld Packets Proceeded\n", packet_count);
    }
    free(entry);

    fclose(dump_fp);
    fclose(csv_fp);
    fclose(csv_fp_summary);
    closedir(dir);
    printf("All Packets Proceeded\n");

    // Pythonスクリプトをsystemで起動
    printf("Launching Python Plot...\n");
    int ret = system("python3 plot_rtp.py");
    if (ret != 0) {
        fprintf(stderr, "Python script failed with code %d\n", ret);
    }

    return 0;
}
