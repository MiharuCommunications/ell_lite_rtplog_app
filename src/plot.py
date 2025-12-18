
import datetime
import os
import sys
import glob
import struct
from plotly.subplots import make_subplots
import plotly.graph_objects as go
from plotly_resampler import register_plotly_resampler
import pandas as pd
from threading import Timer
import webbrowser

# Get RTP Log Files
args = sys.argv
if 1 == len(args):
    log_dir = "rtp-log"
else:
    log_dir = args[1]

log_files = log_dir + "/*.bin"
logs = glob.glob(log_files)
logs.sort()

if 0 == len(logs):
    sys.exit("Invalid LogFileDirectory : %s" % args[1])

register_plotly_resampler(mode='auto')

# init 
rtp_sn = 0 
rtp_sn_prev = 0 

snd_ts = 0
snd_ts_prev = 0

rcv_ts = 0
rcv_ts_prev = 0

pkt_time_t = 0
pkt_time = {}

d = 0
d_prev = 0
d_max = 0

j = 0
j_prev = 0
j_max = 0

dus = {}
jus = {}
loss = {}
dsize = 18

dus[0] = 0
jus[0] = 0
loss[0] = 0

i = 0
i_sec = 0
total_i = 0
loss_t = 0

date_format = "%Y%m%d_%H%M"
pkt_itv = datetime.timedelta(milliseconds=1) 

# Log Analysis
for log in logs:
    print("Processing %s" %(log))

    # Get date from FileName
    date_s = os.path.splitext(os.path.basename(log))[0]
    date = datetime.datetime.strptime(date_s, date_format)

    # UTC to JST(+9 hours)
    date = date + datetime.timedelta(hours=9)
    pkt_time_t = date

    with open(log, 'rb') as f:
        data = f.read()
        file_size = len(data)
        ofs = 0
    
        while ofs+(dsize-1) < file_size:
            i = total_i + (ofs / dsize)
    
            rtp_sn_prev = rtp_sn
            rtp_sn_t    = struct.unpack_from('<H', data, ofs)
            rtp_sn      = rtp_sn_t[0]
    
            snd_ts_prev = snd_ts
            snd_ts_sec  = struct.unpack_from('<L', data, ofs+2)
            snd_ts_nsec = struct.unpack_from('<L', data, ofs+6)
            snd_ts      = snd_ts_sec[0] + snd_ts_nsec[0] * pow(10, -9)
    
            rcv_ts_prev = rcv_ts
            rcv_ts_sec  = struct.unpack_from('<L', data, ofs+10)
            rcv_ts_nsec = struct.unpack_from('<L', data, ofs+14)
            rcv_ts    = rcv_ts_sec[0] + rcv_ts_nsec[0] * pow(10, -9)
            ofs = ofs + dsize
    
            d_prev = d
            j_prev = j

            if i > 0:
                # Packet Loss Analysis
                if (rtp_sn > 0 and rtp_sn - rtp_sn_prev != 1) or (rtp_sn == 0 and rtp_sn_prev != 65535) :
                    loss_t = loss_t + 1
                else:
                    # Jitter Analysis
                    d = abs((rcv_ts - rcv_ts_prev) - (snd_ts - snd_ts_prev))
                    j = j_prev + (d_prev - j_prev) / 16
                    #j = j_prev + (d_prev - j_prev)
                    #print(j)
                    if (d_max < d) :
                        d_max = d
                    if (j_max < j) :
                        j_max = j

                if i%1000 == 0:
                    pkt_time[i_sec] = pkt_time_t
                    dus[i_sec] = d_max * 1000 * 1000
                    jus[i_sec] = j_max * 1000 * 1000
                    loss[i_sec] = loss_t
                    i_sec = i_sec + 1
                    d_max = 0
                    j_max = 0
                    loss_t = 0

            if i > total_i:
                pkt_time_t = pkt_time_t + pkt_itv

    total_i = i + 1
    print("%d Packets Proceeded" %(total_i) )

print("All Packets Proceeded")

# Plot
print("Drawing Graph")
#fig = go.Figure()
fig = make_subplots(rows=2, cols=1, shared_xaxes=True, subplot_titles=["Jitter", "Packet Loss"], vertical_spacing=0.03)
#fig = make_subplots(rows=1, cols=1, shared_xaxes=True)
fig.add_trace(go.Scatter(x=list(pkt_time.values()), y=list(dus.values()), name="Jitter(moment)", mode="lines"), row=1, col=1)
fig.add_trace(go.Scatter(x=list(pkt_time.values()), y=list(jus.values()), name="Jitter(average)", mode="lines"), row=1, col=1)
fig.add_trace(go.Scatter(x=list(pkt_time.values()), y=list(loss.values()), name="Paket loss", mode="lines"), row=2, col=1)
#fig.update_xaxes(title = "Time", row=1,col=1)
#fig.update_xaxes(title = "Time", row=2,col=1)
fig.update_yaxes(title = "Jitter(us)", row=1, range=[None,None])
fig.update_yaxes(title = "Packet Loss (Packets/Sec)", row=2, range=[None,None])
port = 8050
Timer(1, lambda: webbrowser.open_new(f"http://localhost:{port}")).start()
fig.show_dash(mode="inline", port=port)
print("Drawing Graph Completed")

