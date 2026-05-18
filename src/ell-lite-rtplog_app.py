import struct
import datetime
import ctypes
import sys
from plotly.subplots import make_subplots
import plotly.graph_objects as go
#from plotly_resampler import register_plotly_resampler
import webbrowser
from threading import Timer

#register_plotly_resampler(mode='auto')

record_format = "<6idd5i"  # packed: 6 ints, 2 doubles, 4 ints
record_size = struct.calcsize(record_format)

pkt_time = {}
dms = {}
jms = {}
loss = {}
loss_burst = {}
duplicate = {}
reorder_count = {}
reorder_length = {}

filename = "rtp_dump.bin"

args = sys.argv
if(1 == len(args)) :
    log_dir = b"rtp-log"
else :
    log_dir = args[1].encode('utf-8')

# Generate rtp_dump.bin
anlys = ctypes.CDLL("ell-lite-rtplog_anlys.so")
anlys.log_anlys(ctypes.c_char_p(log_dir))

with open(filename, "rb") as f:
    i_sec = 0
    while True:
        data = f.read(record_size)
        if not data:
            break
        year, month, day, hour, minute, second, d_max_ms, j_max_ms, loss_t, loss_burst_t, duplicate_t, reorder_count_t, reorder_length_t = struct.unpack(record_format, data)
        #print(year)
        #print(month)
        #print(day)
        #print(hour)
        #print(minute)
        #print(second)

        # UTC → JST (+9時間)
        dt = datetime.datetime(year, month, day, hour, minute, second)
        #dt_jst = dt + datetime.timedelta(hours=9)
        dt_jst = dt

        #if(d_max_ms > 100) :
        #    print("Jitter,%d,ms,%s" %(int(d_max_ms), dt_jst))
        #    #print("[Jitter] %d ms (%s)" %(int(d_max_ms), dt_jst))

        #if(loss_t > 0) :
        #    print("Loss,%d,pkts/sec,%s" %(loss_t, dt_jst))
        #    #print("[Loss] %d pkts/sec (%s)" %(loss_t, dt_jst))

        #if(duplicate_t > 0) :
        #    print("Duplicate,%d,pkts/sec,%s" %(duplicate_t, dt_jst))
        #    #print("[Duplicate] %d pkts/sec (%s)" %(duplicate_t, dt_jst))

        pkt_time[i_sec] = dt_jst
        dms[i_sec] = d_max_ms
        jms[i_sec] = j_max_ms
        loss[i_sec] = loss_t
        loss_burst[i_sec] = loss_burst_t
        duplicate[i_sec] = duplicate_t
        reorder_count[i_sec] = reorder_count_t
        reorder_length[i_sec] = reorder_length_t
        i_sec += 1

print("Drawing Graph")
fig = make_subplots(rows=2, cols=1, shared_xaxes=True,
                    subplot_titles=["Jitter (ms)", "Loss/Duplicate/Reordering (pkt/sec)"], vertical_spacing=0.03)

fig.add_trace(go.Scattergl(x=list(pkt_time.values()), y=list(dms.values()), name="Jitter(moment)", mode="lines+markers"), row=1, col=1)
fig.add_trace(go.Scattergl(x=list(pkt_time.values()), y=list(jms.values()), name="Jitter(average)", mode="lines+markers"), row=1, col=1)
fig.add_trace(go.Scattergl(x=list(pkt_time.values()), y=list(loss.values()), name="Loss(total)", mode="lines+markers"), row=2, col=1)
fig.add_trace(go.Scattergl(x=list(pkt_time.values()), y=list(loss_burst.values()), name="Loss(burst)", mode="lines+markers"), row=2, col=1)
fig.add_trace(go.Scattergl(x=list(pkt_time.values()), y=list(duplicate.values()), name="Duplicate", mode="lines+markers"), row=2, col=1)
fig.add_trace(go.Scattergl(x=list(pkt_time.values()), y=list(reorder_count.values()), name="Reordering(count)", mode="lines+markers"), row=2, col=1)
fig.add_trace(go.Scattergl(x=list(pkt_time.values()), y=list(reorder_length.values()), name="Reordering(length)", mode="lines+markers"), row=2, col=1)

#fig.update_yaxes(title="Jitter(ms)", row=1)
#fig.update_yaxes(title="Packets(pkt/sec)", row=2)
#fig.update_xaxes(rangeslider_visible=True)

fig.write_html("rtp_plot.html")

#port = 8050
#Timer(1, lambda: webbrowser.open_new(f"http://localhost:{port}")).start()
#fig.show_dash(mode="internal", port=port)
print("Plotted %d samples" %(len(pkt_time)))
print("Drawing Graph Completed")
