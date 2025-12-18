import struct
import datetime
from plotly.subplots import make_subplots
import plotly.graph_objects as go
from plotly_resampler import register_plotly_resampler
import webbrowser
from threading import Timer

register_plotly_resampler(mode='auto')

record_format = "<6iddii"  # packed: 6 ints, 2 doubles, 2 ints
record_size = struct.calcsize(record_format)

pkt_time = {}
dus = {}
jus = {}
loss = {}
duplicate = {}

filename = "rtp_dump.bin"

with open(filename, "rb") as f:
    i_sec = 0
    while True:
        data = f.read(record_size)
        if not data:
            break
        year, month, day, hour, minute, second, d_max_us, j_max_us, loss_t, duplicate_t = struct.unpack(record_format, data)
        #print(year)
        #print(month)
        #print(day)
        #print(hour)
        #print(minute)
        #print(second)

        # UTC → JST (+9時間)
        dt = datetime.datetime(year, month, day, hour, minute, second)
        dt_jst = dt + datetime.timedelta(hours=9)

        pkt_time[i_sec] = dt_jst
        dus[i_sec] = d_max_us
        jus[i_sec] = j_max_us
        loss[i_sec] = loss_t
        duplicate[i_sec] = duplicate_t
        i_sec += 1

print("Drawing Graph")
fig = make_subplots(rows=2, cols=1, shared_xaxes=True,
                    subplot_titles=["Jitter", "Packet Loss/Duplicate"], vertical_spacing=0.03)

fig.add_trace(go.Scatter(x=list(pkt_time.values()), y=list(dus.values()), name="Jitter(moment)", mode="lines"), row=1, col=1)
fig.add_trace(go.Scatter(x=list(pkt_time.values()), y=list(jus.values()), name="Jitter(average)", mode="lines"), row=1, col=1)
fig.add_trace(go.Scatter(x=list(pkt_time.values()), y=list(loss.values()), name="Packet loss", mode="lines"), row=2, col=1)
fig.add_trace(go.Scatter(x=list(pkt_time.values()), y=list(duplicate.values()), name="Pakcket duplicate", mode="lines"), row=2, col=1)

fig.update_yaxes(title="Jitter(us)", row=1)
fig.update_yaxes(title="Packet Loss/Duplicate (Packets/Sec)", row=2)

port = 8050
Timer(1, lambda: webbrowser.open_new(f"http://localhost:{port}")).start()
fig.show_dash(mode="inline", port=port)
print("Drawing Graph Completed")
