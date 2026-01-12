import pandas as pd;

data = pd.read_csv('trace.csv');
print(data.columns);
data['Latency (ms)'] = data['Latency (ms)']/2;
data.to_csv('trace.csv');