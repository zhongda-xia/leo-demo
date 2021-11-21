#!/usr/bin/env Rscript

suppressMessages (library(ggplot2))
## suppressMessages (library(doBy))

# source ("graphs/graph-style.R")

inputLoss = "results/kite/loss.txt"
outputLoss = "graphs/pdfs/kite-loss.pdf"

dataLoss <- read.table (file(inputLoss, "r"), header=TRUE)

dataLoss <- subset(dataLoss, run == 2)
# print(dataLoss)

dataLoss$traceLifetime <- as.factor(dataLoss$traceLifetime)

dataLoss.mean <- aggregate(x=dataLoss["Loss"], by=c(dataLoss["refreshInterval"], dataLoss["traceLifetime"],
  dataLoss["solution"], dataLoss["strategy"], dataLoss["doPull"], dataLoss["speed"]), FUN=mean)
print(dataLoss.mean)

speeds <- subset(dataLoss.mean, solution == "kite" & strategy == "multicast" & doPull == "false")
print(speeds)

# g <- ggplot(speeds, aes(x=refreshInterval, y=Loss, group=traceLifetime)) +
#      geom_line(aes(color=traceLifetime)) +
#      scale_y_continuous ("Loss rate", limits = c(0, NA)) +
#      scale_x_discrete ("Refresh interval, m/s") +
#      facet_grid(speed~., scales = "free")
#      # + theme_custom ()

speeds$refreshInterval <- as.factor(speeds$refreshInterval)

g <- ggplot(speeds, aes(x=traceLifetime, y=Loss, group=refreshInterval)) +
     geom_line(aes(color=refreshInterval)) +
     scale_y_continuous ("Loss rate", limits = c(0, NA)) +
     scale_x_discrete ("Trace lifetime, ms") +
     facet_grid(speed~., scales = "free")
     # + theme_custom ()

if (!file.exists ("graphs/pdfs")) {
  dir.create ("graphs/pdfs")
}

pdf (outputLoss, width=5, height=3.5)
g
x = dev.off ()
