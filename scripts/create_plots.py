# I need to read a csv file and use pivot tables
# to create a plot of the data
# I will use the pandas library to read the csv file

import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import seaborn as sns
import os

# Set Seaborn style
sns.set(style="whitegrid")

# Increase font size
plt.rcParams.update({'font.size': 14})
# Read the csv file into a dataframe
orca_art = """
                                       _
                                     / |
                                    /  |
                                   /   |
                                  /    |
                ,____.-----------'     `.______,                         ._
           ._--"                                "-----.______,          / /
         ,/__o_~##mm                                          "-----__./  |
         ==--    ~~                                 ____               \_ |
          "~###**~~~|   \                    ./####"            _________(
               ~~~##|    |~~~      _________/###/                /###\.   |
                    (    |##################~__________--------~~~~~~~~\  |
                     \   |  ~~###########~~~~~~                         \._
                      \__\
            :- Orcinus Orca wishes a nice summer  -:
"""
summer_art = """
          ... ....                     .. ...                           ____,--
  .. ...... .                           ........__                  __,'MMII;:.
...                                      ......|__|            _,--'MMI;:.
                                    @         /|. .          ,'MMI;:.WI;;:.
            ,d888b,                |.\       / |\           /MWI;;  WWI;.
           J8888888L               |' \     /^^|^\        ,'MWI;   WWWI;;:.
           888888888               |. o\ __/___|__\_   ,-'MWI;:.  WI;;::.
-----------------------------------|.  L\-`--------'--'_MWI;:.   WWI;;:.
      - -__--__--__--__ -           \.   \                `---. WI;:.     ____-
       - __--__--__-- _             |:    :.                   `/|-------'
        _ -__--__--_ -              |:     ::.                ,''/
          - _--__- _                |/       ::.             /: |
___        _ --__ -                 /'         :`--.______,-::  /
###\        _ -_ -                 /'   ._ .     ``        '    `-_
,--'         --__              _,-'   __/. .... .  .  ___,---.__,-'-.
              -_         __,--/'   __/##`-._____,----'::::::::::::::#\
                              `---'`````##:::::::::::::::::::::::::::#`---.
With sun and seals                                      `````::::::::::::#######:::####\
//

"""

def print_separator():
    print("\n===============================================================\n")




print(orca_art) 

desired_order = [
    "bc_graph500-scale23-ef16_adj",
    "bfs_CL-10M-1d8-L5",
    "cc_CL-10M-1d8-L5",
    "dlrm",
    "genomicsbench",
    "graphcoloring_graph500-scale23-ef16_adj",
    "pagerank_CL-10M-1d8-L5",
    "randacc_1G",
    "sssp_graph500-scale23-ef16_adj",
    "tc_CL-10M-1d8-L5",
    "XSBench_15000"
]

desired_order_labels = [
    "bc",
    "bfs",
    "cc",
    "dlrm",
    "gen",
    "gc",
    "pr",
    "rnd",
    "sssp",
    "tc",
    "xs"
]
desired_order_labels_gmean = [
    "BC",
    "BFS",
    "CC",
    "DLRM",
    "GEN",
    "GC",
    "PR",
    "RND",
    "SSSP",
    "TC",
    "XS",
    "GMEAN"
]

print_separator()
print("Plotting Figure 2: L2 TLB MPKI for different traces")
print_separator()
# Reindex the DataFrame to change the order of experiments

df = pd.read_csv('./results.csv')

# Create a pivot table of the data
selected_experiments = ['tlb_base_ideal',
                        'tlb_2x_ideal',
                        'tlb_4x_ideal',
                        'tlb_8x_ideal',
                        'tlb_16x_ideal',
                        'tlb_32x_ideal',
                        'tlb_64x_ideal']

df['stlb.mpki'] = (df['stlb.miss'] * 1000) / df['performance_model.instruction_count']

# Filter the DataFrame for the selected experiments
df_selected = df[df['Exp'].isin(selected_experiments)]

# Create the pivot table with the selected experiments and GMEAN
geometric_mean = df_selected.groupby('Exp')['stlb.mpki'].apply(lambda x: np.prod(x) ** (1 / len(x)))


pivot_table_fig2 = df_selected.pivot_table(index='Trace', columns='Exp', values='stlb.mpki')
print(pivot_table_fig2)
pivot_table_fig2 = pivot_table_fig2.reindex(desired_order_labels)
pivot_table_fig2.loc['GMEAN'] = geometric_mean.values

# Sort the columns based on the 'selected_experiments' list
pivot_table_fig2 = pivot_table_fig2[selected_experiments]

ax = pivot_table_fig2.plot(kind='bar',figsize=(12, 3))
ax.set_xticklabels(desired_order_labels_gmean)

plt.xlabel('Trace')
plt.ylabel('L2 TLB MPKI')
plt.title('Figure 2: L2 TLB MPKI for different TLB sizes')
plt.legend(title='Configuration', bbox_to_anchor=(1.05, 1), loc='upper left')
plt.tight_layout()

# Replace 'output_path.png' with the desired file path and name for the saved image
plt.savefig('./plots/figure2.png')

print_separator()
print("Plotting Figure 3: L2 TLB Performance for different traces")
print_separator()


pivot_table_fig3 = df_selected.pivot_table(index='Trace', columns='Exp', values='performance_model.cycle_count')

#print pivot_table_fig3


# you need to divide each column by the first column
pivot_table_fig3 = pivot_table_fig3.rdiv(pivot_table_fig3['tlb_base_ideal'], axis=0)
pivot_table_fig3 = pivot_table_fig3[selected_experiments]

geometric_mean_perf = pivot_table_fig3.apply(lambda x: np.prod(x) ** (1 / len(x)))
pivot_table_fig3 = pivot_table_fig3.reindex(desired_order_labels)

pivot_table_fig3.loc['GMEAN'] = geometric_mean_perf.values

# Drop the "Exp1" column as it will always be 1 in the relative pivot table

pivot_table_fig3.drop('tlb_base_ideal', axis=1, inplace=True)

print(pivot_table_fig3)

ax = pivot_table_fig3.plot(kind='bar',figsize=(12, 3))
ax.set_xticklabels(desired_order_labels_gmean)

plt.xlabel('Trace')
plt.ylabel('Normalized Speedup')
plt.title('Figure 3: Normalized Speedup for different L2 TLB sizes')
plt.legend(title='Configuration', bbox_to_anchor=(1.05, 1), loc='upper left')
plt.ylim(0.9, 1.15)

plt.tight_layout()

# Replace 'output_path.png' with the desired file path and name for the saved image
plt.savefig('./plots/figure3.png')


print_separator()
print("Plotting Figure 4: L3 TLB Performance for different traces")
print_separator()

selected_experiments = [
                        'tlb_base_ideal',
                        'L3TLB_ideal_15',
                        'L3TLB_ideal_20',
                        'L3TLB_ideal_25',
                        'L3TLB_ideal_30',
                        'L3TLB_ideal_35',
                        'L3TLB_ideal_40'
]

df_selected = df[df['Exp'].isin(selected_experiments)]

pivot_table_fig4 = df_selected.pivot_table(index='Trace', columns='Exp', values='performance_model.cycle_count')



# you need to divide each column by the first column
pivot_table_fig4 = pivot_table_fig4.rdiv(pivot_table_fig4['tlb_base_ideal'], axis=0)
pivot_table_fig4 = pivot_table_fig4[selected_experiments]
pivot_table_fig4 = pivot_table_fig4.reindex(desired_order_labels)

geometric_mean_perf = pivot_table_fig4.apply(lambda x: np.prod(x) ** (1 / len(x)))
pivot_table_fig4.loc['GMEAN'] = geometric_mean_perf.values

# Drop the "Exp1" column as it will always be 1 in the relative pivot table

pivot_table_fig4.drop('tlb_base_ideal', axis=1, inplace=True)

print(pivot_table_fig4)

ax = pivot_table_fig4.plot(kind='bar',figsize=(12, 3))
ax.set_xticklabels(desired_order_labels_gmean)

plt.xlabel('Trace')
plt.ylabel('Normalized Speedup')
plt.title('Figure 4: Normalized Speedup for different L3 TLB sizes')
plt.legend(title='Configuration', bbox_to_anchor=(1.05, 1), loc='upper left')
plt.ylim(0.9, 1.15)

plt.tight_layout()

# Replace 'output_path.png' with the desired file path and name for the saved image
plt.savefig('./plots/figure4.png')


print_separator()
print("Plotting Figure 6: L2 Cache Data Reuse for different traces")
print_separator()

selected_experiments = [
                    'tlb_base_ideal', 
]

selected_metrics = [
    "L2.data-reuse-0",
    "L2.data-reuse-1",
    "L2.data-reuse-2",
    "L2.data-reuse-3",
    "L2.data-reuse-4",
]

df_selected = df[df['Exp'].isin(selected_experiments)]

pivot_table_fig6 = df_selected.pivot_table(index='Trace', columns='Exp', values=selected_metrics)
pivot_table_fig6 = pivot_table_fig6.div(pivot_table_fig6.sum(axis=1), axis=0) * 100
pivot_table_fig6 = pivot_table_fig6.reindex(desired_order_labels)

#calculate the geometric mean
pivot_table_fig6.loc['MEAN'] = pivot_table_fig6.mean(axis=0).values


print(pivot_table_fig6)

ax = pivot_table_fig6.plot(kind='bar', stacked=True, figsize=(15, 3))
ax.set_xticklabels(desired_order_labels_gmean)

plt.title('Figure 6: Reuse of L2 cache blocks')
plt.xlabel('Traces')
plt.ylabel('Breakdown of reuse (%)')

plt.tight_layout()
plt.ylim(80, 100)
plt.legend( title="Reuse", bbox_to_anchor=(1.5, 1.0))
# Replace 'output_path.png' with the desired file path and name for the saved image
plt.savefig('./plots/figure6.png')

print_separator()
print("Plotting Figure 15: Normalized Speedup for different configurations")
print_separator()

selected_experiments = ['tlb_base_ideal',
                        'pomtlb_64K',
                        'L3TLB_ideal_15',
                        'tlb_64x_ideal',
                        'tlb_128x_ideal',
                        'victima_ptw_2MBL2']

df_selected = df[df['Exp'].isin(selected_experiments)]

pivot_table_fig15 = df_selected.pivot_table(index='Trace', columns='Exp', values='performance_model.cycle_count')
pivot_table_fig15 = pivot_table_fig15.rdiv(pivot_table_fig15['tlb_base_ideal'], axis=0)

pivot_table_fig15 = pivot_table_fig15[selected_experiments]
pivot_table_fig15 = pivot_table_fig15.reindex(desired_order_labels)

geometric_mean_perf = pivot_table_fig15.apply(lambda x: np.prod(x) ** (1 / len(x)))
pivot_table_fig15.loc['GMEAN'] = geometric_mean_perf.values

print(pivot_table_fig15)
# Drop the "Exp1" column as it will always be 1 in the relative pivot table

pivot_table_fig15.drop('tlb_base_ideal', axis=1, inplace=True)
ax = pivot_table_fig15.plot(kind='bar',figsize=(12, 3))

ax.set_xticklabels(desired_order_labels_gmean)

plt.xlabel('Trace')
plt.ylabel('Normalized Speedup')
plt.title('Figure 15: Normalized Speedup')
plt.legend(title='Configuration', bbox_to_anchor=(1.05, 1), loc='upper left')
plt.ylim(0.9, 1.3)

plt.tight_layout()
plt.savefig('./plots/figure15.png')

print_separator()
print("Plotting Figure 16: PTW Reduction for different configurations")
print_separator()

selected_experiments = ['tlb_base_ideal',
                        'pomtlb_64K',
                        'tlb_64x_ideal',
                        'tlb_128x_ideal',
                        'victima_ptw_2MBL2']

df_selected = df[df['Exp'].isin(selected_experiments)]

pivot_table_fig16 = df_selected.pivot_table(index='Trace', columns='Exp', values='PTW_0.page_walks')
pivot_table_fig16_subbed = pivot_table_fig16.rsub(pivot_table_fig16['tlb_base_ideal'], axis=0)
pivot_table_fig16_subbed = pivot_table_fig16.div(pivot_table_fig16['tlb_base_ideal'], axis=0)
pivot_table_fig16_subbed *= 100

pivot_table_fig16 = pivot_table_fig16.reindex(desired_order_labels)

geometric_mean_perf = pivot_table_fig16_subbed.apply(lambda x: np.prod(x) ** (1 / len(x)))

pivot_table_fig16_subbed.loc['GMEAN'] = geometric_mean_perf.values
pivot_table_fig16_subbed = pivot_table_fig16_subbed[selected_experiments]
print(pivot_table_fig16_subbed)
# Drop the "Exp1" column as it will always be 1 in the relative pivot table

pivot_table_fig16_subbed.drop('tlb_base_ideal', axis=1, inplace=True)

ax = pivot_table_fig16_subbed.plot(kind='bar',figsize=(12, 3))

ax.set_xticklabels(desired_order_labels_gmean)

plt.xlabel('Trace')
plt.ylabel('PTW Reduction (%)')
plt.title('Figure 16: Reduction of PTWs across different configurations')
plt.legend(title='Configuration', bbox_to_anchor=(1.05, 1), loc='upper left')

plt.tight_layout()
plt.savefig('./plots/figure16.png')

print_separator()
print("Plotting Figure 19: L2 Cache TLB Reuse with Victima")
print_separator()

selected_experiments = [
                    'victima_ptw_2MBL2' 
]

selected_metrics = [
    "L2.tlb-reuse-0",
    "L2.tlb-reuse-1",
    "L2.tlb-reuse-2",
    "L2.tlb-reuse-3",
    "L2.tlb-reuse-4",
]

df_selected = df[df['Exp'].isin(selected_experiments)]

pivot_table_fig19 = df_selected.pivot_table(index='Trace', columns='Exp', values=selected_metrics)
print(pivot_table_fig19)

pivot_table_fig19 = pivot_table_fig19.div(pivot_table_fig19.sum(axis=1), axis=0) * 100
pivot_table_fig19 = pivot_table_fig19.reindex(desired_order_labels)

pivot_table_fig19.loc['GMEAN'] = pivot_table_fig19.mean()



ax = pivot_table_fig19.plot(kind='bar', stacked=True, figsize=(15, 3))

ax.set_xticklabels(desired_order_labels_gmean)


plt.title('Figure 19: L2 Cache TLB Reuse with Victima')
plt.xlabel('Traces')
plt.ylabel('Breakdown of L2 TLB Reuse (%)')
plt.legend(loc='upper right', title='Reuse', bbox_to_anchor=(1.55, 1))

plt.tight_layout()
plt.ylim(0, 100)

# Replace 'output_path.png' with the desired file path and name for the saved image
plt.savefig('./plots/figure19.png')

print_separator()
print("Plotting Figure 20: PTW Reduction across different L2 Cache Sizes")
print_separator()

selected_experiments = ['baseline_radix_1MB',
                        'baseline_radix_2MB',
                        'baseline_radix_4MB',
                        'baseline_radix_8MB',
                        'victima_ptw_1MBL2',
                        'victima_ptw_2MBL2',
                        'victima_ptw_4MBL2',
                        'victima_ptw_8MBL2'
                        ]

df_selected = df[df['Exp'].isin(selected_experiments)]

pivot_table_fig20 = df_selected.pivot_table(index='Trace', columns='Exp', values='PTW_0.page_walks')
pivot_table_fig20['1MB'] = (pivot_table_fig20['baseline_radix_1MB'] - pivot_table_fig20['victima_ptw_1MBL2'])/pivot_table_fig20['baseline_radix_1MB']*100
pivot_table_fig20['2MB'] = (pivot_table_fig20['baseline_radix_2MB'] - pivot_table_fig20['victima_ptw_2MBL2'])/pivot_table_fig20['baseline_radix_2MB']*100
pivot_table_fig20['4MB'] = (pivot_table_fig20['baseline_radix_4MB'] - pivot_table_fig20['victima_ptw_4MBL2'])/pivot_table_fig20['baseline_radix_4MB']*100
pivot_table_fig20['8MB'] = (pivot_table_fig20['baseline_radix_8MB'] - pivot_table_fig20['victima_ptw_8MBL2'])/pivot_table_fig20['baseline_radix_8MB']*100

pivot_table_fig20.drop('baseline_radix_1MB', axis=1, inplace=True)
pivot_table_fig20.drop('baseline_radix_2MB', axis=1, inplace=True)
pivot_table_fig20.drop('baseline_radix_4MB', axis=1, inplace=True)
pivot_table_fig20.drop('baseline_radix_8MB', axis=1, inplace=True)

pivot_table_fig20.drop('victima_ptw_1MBL2', axis=1, inplace=True)
pivot_table_fig20.drop('victima_ptw_2MBL2', axis=1, inplace=True)
pivot_table_fig20.drop('victima_ptw_4MBL2', axis=1, inplace=True)
pivot_table_fig20.drop('victima_ptw_8MBL2', axis=1, inplace=True)


geometric_mean_perf = pivot_table_fig20.apply(lambda x: np.prod(x) ** (1 / len(x)))
pivot_table_fig20.loc['GMEAN'] = geometric_mean_perf.values

# Drop the "Exp1" column as it will always be 1 in the relative pivot table

print(pivot_table_fig20)

ax = pivot_table_fig20.plot(kind='bar',figsize=(12, 3))

ax.set_xticklabels(desired_order_labels_gmean)

plt.xlabel('Trace')
plt.ylabel('Reduction of PTWs (%)')
plt.title('Figure 20: Reduction of PTWs across different L2 Cache Sizes')
plt.legend(title='Configuration', bbox_to_anchor=(1.05, 1), loc='upper left')

plt.tight_layout()
plt.savefig('./plots/figure20.png')

print_separator()
print("Plotting Figure 22: Speedup in virtualized environments")
print_separator()

selected_experiments = [
    'baseline_radix_virtualized',
    'potm_virtualized',
    'tlb_base_ideal',
    'victima_virtualized'
]

df_selected = df[df['Exp'].isin(selected_experiments)]

pivot_table_fig22 = df_selected.pivot_table(index='Trace', columns='Exp', values='performance_model.cycle_count')
pivot_table_fig22 = pivot_table_fig22.rdiv(pivot_table_fig22['baseline_radix_virtualized'], axis=0)

pivot_table_fig22 = pivot_table_fig22[selected_experiments]
pivot_table_fig22 = pivot_table_fig22.reindex(desired_order_labels)

geometric_mean_perf = pivot_table_fig22.apply(lambda x: np.prod(x) ** (1 / len(x)))
pivot_table_fig22.loc['GMEAN'] = geometric_mean_perf.values

# Drop the "Exp1" column as it will always be 1 in the relative pivot table

pivot_table_fig22.drop('baseline_radix_virtualized', axis=1, inplace=True)
print(pivot_table_fig22)
ax = pivot_table_fig22.plot(kind='bar',figsize=(12, 3))
ax.set_xticklabels(desired_order_labels_gmean)

plt.xlabel('Trace')
plt.ylabel('Normalized Speedup')
plt.title('Figure 22: Normalized Speedup for different configurations')
plt.legend(title='Experiment', bbox_to_anchor=(1.05, 1), loc='upper left')
plt.ylim(0.9, 1.3)

plt.tight_layout()
plt.savefig('./plots/figure22.png')

print_separator()
print("Plotting Figure 23: Host and Guest PTW Reduction in virtualized environments")
print_separator()

selected_experiments = [
    'baseline_radix_virtualized',
    'potm_virtualized',
    'victima_virtualized'
]

df_selected = df[df['Exp'].isin(selected_experiments)]

pivot_table_fig23 = df_selected.pivot_table(index='Trace', columns='Exp', values=['PTW_1.page_walks', 'PTW_2.page_walks'])

ptw_baseline = pivot_table_fig23[('PTW_1.page_walks', 'baseline_radix_virtualized')]
ptw_ptw1_pomtlb = pivot_table_fig23[('PTW_1.page_walks', 'potm_virtualized')]
ptw_ptw1_victima = pivot_table_fig23[('PTW_1.page_walks', 'victima_virtualized')]

# Calculate the percentage difference between PTW_1.page_walks and baseline_radix_virtualized
pivot_table_fig23[('Guest','potm')] = (((ptw_baseline-ptw_ptw1_pomtlb) / ptw_baseline) * 100)
pivot_table_fig23[('Guest','victima')] = (((ptw_baseline-ptw_ptw1_victima) / ptw_baseline) * 100)

ptw_baseline = pivot_table_fig23[('PTW_2.page_walks', 'baseline_radix_virtualized')]
ptw_ptw2_pomtlb = pivot_table_fig23[('PTW_2.page_walks', 'potm_virtualized')]
ptw_ptw2_victima = pivot_table_fig23[('PTW_2.page_walks', 'victima_virtualized')]

# Calculate the percentage difference between PTW_1.page_walks and baseline_radix_virtualized
pivot_table_fig23[('Host','POM-TLB')] = (((ptw_baseline-ptw_ptw2_pomtlb) / ptw_baseline) * 100)
pivot_table_fig23[('Host','Victima')] = (((ptw_baseline-ptw_ptw2_victima) / ptw_baseline) * 100)

#calculate the geometric mean
geometric_mean_perf = pivot_table_fig23.apply(lambda x: np.prod(x) ** (1 / len(x)))
pivot_table_fig23.loc['GMEAN'] = geometric_mean_perf.values

pivot_table_fig23.drop(('PTW_1.page_walks', 'baseline_radix_virtualized'), axis=1, inplace=True)
pivot_table_fig23.drop(('PTW_2.page_walks', 'baseline_radix_virtualized'), axis=1, inplace=True)

pivot_table_fig23.drop(('PTW_1.page_walks', 'potm_virtualized'), axis=1, inplace=True)
pivot_table_fig23.drop(('PTW_2.page_walks', 'potm_virtualized'), axis=1, inplace=True)

pivot_table_fig23.drop(('PTW_1.page_walks', 'victima_virtualized'), axis=1, inplace=True)
pivot_table_fig23.drop(('PTW_2.page_walks', 'victima_virtualized'), axis=1, inplace=True)

print(pivot_table_fig23)

ax = pivot_table_fig23.plot(kind='bar',figsize=(12, 3))
ax.set_xticklabels(desired_order_labels_gmean)

plt.xlabel('Trace')
plt.ylabel('PTW Reduction (%)')
plt.title('Figure 23: Host and Guest PTW Reduction in virtualized environments')
plt.legend(title='Configurations', bbox_to_anchor=(1.05, 1), loc='upper left')

plt.tight_layout()
plt.savefig('./plots/figure23.png')

print_separator()
print ("Plotting Figure 18: Memory Reach provided by Victima")
print_separator()


tlb_blocks = {}
for folder in sorted(os.listdir("./results")):
    if("victima_ptw_2MBL2" in folder):
        l2 = 0
        with open("./results/"+folder+"/l2_cache_tlb0") as l2_tlb_file:
                lines = l2_tlb_file.readlines()
                for line in lines:
                    l2 += int(line)
                                              
        tlb_blocks[folder] = (l2/len(lines)*(2**12)/(2**20)*8)

tlb_blocks["GMEAN"] = np.prod(list(tlb_blocks.values())) ** (1 / len(tlb_blocks.values()))
print(tlb_blocks)
x_ticks = list(tlb_blocks.keys())
x_ticks = [x.replace("victima_ptw_2MBL2_", "") for x in x_ticks]

#plot tlb blocks
plt.figure(figsize=(12, 5))
plt.bar(range(len(tlb_blocks)), list(tlb_blocks.values()), align='center')
plt.xticks(range(len(tlb_blocks)),x_ticks, rotation=90)
plt.ylabel('Translation Reach \n provided by L2 Cache (MB)')
plt.title('Figure 18: Memory Reach provided by Victima')
plt.tight_layout()
plt.savefig('./plots/figure18.png')

print_separator()
print("\n")
print(summer_art)


