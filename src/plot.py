
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
rtp_sn = {}
snd_ts = {}
rcv_ts = {}
pkt_time = {}
d = {}
j = {}
dus = {}
jus = {}
loss = {}
dsize = 18

d[0] = 0
j[0] = 0
dus[0] = 0
jus[0] = 0
loss[0] = 0

i = 0
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
    pkt_time[total_i] = date

    with open(log, 'rb') as f:
        data = f.read()
        file_size = len(data)
        ofs = 0
    
        while ofs+(dsize-1) < file_size:
            i = total_i + (ofs / dsize)
    
            rtp_sn_t    = struct.unpack_from('<H', data, ofs)
            rtp_sn[i]   = rtp_sn_t[0]
    
            snd_ts_sec  = struct.unpack_from('<L', data, ofs+2)
            snd_ts_nsec = struct.unpack_from('<L', data, ofs+6)
            snd_ts[i]   = snd_ts_sec[0] + snd_ts_nsec[0] * pow(10, -9)
    
            rcv_ts_sec  = struct.unpack_from('<L', data, ofs+10)
            rcv_ts_nsec = struct.unpack_from('<L', data, ofs+14)
            rcv_ts[i] = rcv_ts_sec[0] + rcv_ts_nsec[0] * pow(10, -9)
            #print("%d:%f    %f" %(rtp_sn[i], snd_ts[i], rcv_ts[i]))
            ofs = ofs + dsize
    
            if i > 0:
                d[i] = abs((rcv_ts[i] - rcv_ts[i-1]) - (snd_ts[i] - snd_ts[i-1]))
                j[i] = j[i-1] + (d[i-1] - j[i-1]) / 16
                #j[i] = j[i-1] + (d[i-1] - j[i-1])
                #print(j[i])
                jus[i] = j[i]*1000*1000
                dus[i] = d[i]*1000*1000

                # Packet Loss Analysis
                if (rtp_sn[i] > 0 and rtp_sn[i] - rtp_sn[i-1] != 1) or (rtp_sn[i] == 0 and rtp_sn[i-1] != 65535) :
                    loss_t = loss_t + 1
                else:
                    loss_t = loss_t

                if i%1000 == 0:
                    loss[i] = loss_t
                    loss_t = 0
                else:
                    loss[i] = 0

            if i > total_i:
                pkt_time[i] = pkt_time[i-1] + pkt_itv

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

