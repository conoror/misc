As I'm the only one using this program, I've filtered all support
except for Windows 7 64 bit!

This is a batch file that will check:
  - For the service stack updates
  - For the July 2016 rollup
  - For subsequent security only rollups

and download the next patch you should install. As this is a batch
file it needs to be run from a cmd prompt. Just run it to get
help.

Notes:
- It is a good idea to stop the Windows Update service before running any patch .msu file. There is a possibility that the install gets confused otherwise
- If the install fails, don't retry. Shutdown, reboot, stop Windows Update and then retry.
- After each patch and reboot, give the updater process about 10 minutes to finish applying the changes. Don't rush to put the new patch in. Yes, I know it's annoying. It's MS, of course it's annoying.
- I had one issue with the latest update on one laptop where the install did not complete and the GUI started to get "weird". In a case like this don't try and fix things as you can get into trouble really easily (I had to hard power off). Just shutdown asap.
