#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <time.h>
#include <stdint.h>
#include <math.h>

#define DSIZE 18

typedef struct __attribute__((__packed__)) {
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
    double dus;
    double jus;
    int loss;
    int duplicate;
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
    uint32_t rate = (argc == 2) ? 1 : atoi(argv[2]);
    DIR *dir;
    struct dirent **entry;
    uint64_t packet_count = 0;
    uint16_t rtp_sn_prev = 0, rtp_sn = 0;
    double snd_ts_prev = 0, snd_ts = 0;
    double rcv_ts_prev = 0, rcv_ts = 0;
    double d = 0, j = 0, d_prev = 0, j_prev = 0;
    double d_max = 0, j_max = 0;
    int loss = 0;
    int duplicate = 0;

    if(rate == 0 || rate > 1000) {
        fprintf(stderr, "Invalid DumpRate : %s (range 1 - 1000)\n", argv[2]);
        return 1;
    }
    rate = 1000 / rate;

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

    // UTCに設定
    setenv("TZ", "UTC", 1);
    tzset();

    int file_count = scandir(log_dir, &entry, scanExtract, alphasort);

    for (int i = 0; i < file_count; i++) {
        if (strstr(entry[i]->d_name, ".bin")) {
            char filepath[512];
            snprintf(filepath, sizeof(filepath), "%s/%s", log_dir, entry[i]->d_name);
            printf("Processing %s\n", filepath);

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
            mktime(&tm_info); // TZ=UTCなのでUTCとして扱う
            int ms = 0;
            char is_start = 0;

            while (ofs + DSIZE <= file_size) {
                rtp_sn_prev = rtp_sn;
                memcpy(&rtp_sn, data + ofs, 2);

                uint32_t snd_sec, snd_nsec, rcv_sec, rcv_nsec;
                memcpy(&snd_sec, data + ofs + 2, 4);
                memcpy(&snd_nsec, data + ofs + 6, 4);
                memcpy(&rcv_sec, data + ofs + 10, 4);
                memcpy(&rcv_nsec, data + ofs + 14, 4);

                snd_ts_prev = snd_ts;
                snd_ts = snd_sec + snd_nsec * 1e-9;
                rcv_ts_prev = rcv_ts;
                rcv_ts = rcv_sec + rcv_nsec * 1e-9;

                ofs += DSIZE;

                if (packet_count > 0) {
                    if ( rtp_sn == rtp_sn_prev ) {
                        // Duplicate Packet
                        duplicate++;
                    }
                    else if ((rtp_sn > 0 && rtp_sn - rtp_sn_prev != 1) ||
                             (rtp_sn == 0 && rtp_sn_prev != 65535)) {
                        // Packet Loss
                        loss++;
                        //if(rtp_sn_prev > rtp_sn) {
                        //    printf("packet loss (%d,%d)\n", rtp_sn, rtp_sn_prev);
                        //    printf("%ld:snd:%f(%d,%d), %f\n", packet_count, snd_ts, snd_sec, snd_nsec, snd_ts_prev);
                        //}
                    }
                    else {
                        // Jitter Calculation
                        if(rcv_ts < rcv_ts_prev) {
                            if((uint32_t)rcv_ts_prev == (uint32_t)rcv_ts) rcv_ts = rcv_ts + 1.0;
                        }
                        if(snd_ts < snd_ts_prev) {
                            if((uint32_t)snd_ts_prev == (uint32_t)snd_ts) snd_ts = snd_ts + 1.0;
                        }

                        //if(packet_count == 19301624){
                        //    printf("(%d,%d)%ld:rcv:%f(%d,%d), %f\n", rtp_sn, rtp_sn_prev, packet_count, rcv_ts, rcv_sec, rcv_nsec, rcv_ts_prev);
                        //    printf("(%d,%d)%ld:snd:%f(%d,%d), %f\n", rtp_sn, rtp_sn_prev, packet_count, snd_ts, snd_sec, snd_nsec, snd_ts_prev);
                        //}

                        if(rcv_ts < rcv_ts_prev) {
                            printf("(%d,%d)%ld:rcv:%f(%d,%d), %f\n", rtp_sn, rtp_sn_prev, packet_count, rcv_ts, rcv_sec, rcv_nsec, rcv_ts_prev);
                            printf("(%d,%d)%ld:snd:%f(%d,%d), %f\n", rtp_sn, rtp_sn_prev, packet_count, snd_ts, snd_sec, snd_nsec, snd_ts_prev);
                        }
                        if(snd_ts < snd_ts_prev) {
                            printf("(%d,%d)%ld:rcv:%f(%d,%d), %f\n", rtp_sn, rtp_sn_prev, packet_count, rcv_ts, rcv_sec, rcv_nsec, rcv_ts_prev);
                            printf("(%d,%d)%ld:snd:%f(%d,%d), %f\n", rtp_sn, rtp_sn_prev, packet_count, snd_ts, snd_sec, snd_nsec, snd_ts_prev);
                        }

                        //double rcv_ts_diff = (rcv_ts < rcv_ts_prev) ? (pow(2,32) + rcv_ts) - rcv_ts_prev : rcv_ts - rcv_ts_prev;
                        //double snd_ts_diff = (snd_ts < snd_ts_prev) ? (pow(2,32) + snd_ts) - snd_ts_prev : snd_ts - snd_ts_prev;
                        double rcv_ts_diff = rcv_ts - rcv_ts_prev;
                        double snd_ts_diff = snd_ts - snd_ts_prev;
                        d = fabs(rcv_ts_diff - snd_ts_diff);
                        j = j_prev + (d_prev - j_prev) / 16;
                        if (d > d_max) d_max = d;
                        if (j > j_max) j_max = j;
                    }
                }

                if (packet_count % rate == 0 && packet_count > 0) {
                    RTPStat stat;
                    stat.year = tm_info.tm_year + 1900;
                    stat.month = tm_info.tm_mon + 1;
                    stat.day = tm_info.tm_mday;
                    stat.hour = tm_info.tm_hour;
                    stat.minute = tm_info.tm_min;
                    stat.second = tm_info.tm_sec;
                    stat.dus = d_max * 1e6;
                    stat.jus = j_max * 1e6;
                    stat.loss = loss;
                    stat.duplicate = duplicate;

                    fwrite(&stat, sizeof(RTPStat), 1, dump_fp);

                    d_max = 0;
                    j_max = 0;
                    loss = 0;
                    duplicate = 0;
                }

                d_prev = d;
                j_prev = j;

                // 1ms加算
                if ( is_start )
                    ms++;

                if (ms >= 1000) {
                    ms = 0;
                    tm_info.tm_sec++;
                    mktime(&tm_info); // 正規化（秒→分→時→日→月→年）
                }

                packet_count++;
                is_start = 1;
            }

            free(data);
        }
        free(entry[i]);
        printf("%ld Packets Proceeded\n", packet_count);
    }
    free(entry);

    fclose(dump_fp);
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
