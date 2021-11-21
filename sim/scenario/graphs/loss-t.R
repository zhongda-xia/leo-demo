#!/usr/bin/env Rscript

suppressMessages (library(ggplot2))
## suppressMessages (library(doBy))

# source ("graphs/graph-style.R")

inputLoss = "results.100s/kite/loss.txt"
outputLoss = "graphs/pdfs/kite-loss-t.pdf"

dataLoss <- read.table (file(inputLoss, "r"), header=TRUE)

# dataLoss <- subset(dataLoss, run == 1)
# print(dataLoss)

dataLoss$traceLifetime <- as.factor(dataLoss$traceLifetime)

dataLoss.mean <- aggregate(x=dataLoss["Loss"], by=c(dataLoss["refreshInterval"], dataLoss["traceLifetime"],
  dataLoss["solution"], dataLoss["strategy"], dataLoss["doPull"], dataLoss["speed"]), FUN=mean)
# print(dataLoss.mean)

strategies <- subset(dataLoss.mean, solution == "kite" & speed == "15" & doPull == "true")
print(strategies)

strategiesT <- subset(dataLoss.mean, solution == "kite" & speed == "15" & doPull == "true")
strategiesT.mean <- aggregate(x=strategiesT["Loss"], by=c(strategiesT["strategy"]), FUN=mean)
print(strategiesT.mean)

speeds <- subset(dataLoss.mean, solution == "kite")
speeds.mean <- aggregate(x=speeds["Loss"], by=c(speeds["speed"]), FUN=mean)
print(speeds.mean)

speedst <- subset(dataLoss.mean, solution == "kite" & traceLifetime == refreshInterval)
print(speedst)
speedst.mean <- aggregate(x=speedst["Loss"], by=c(speedst["traceLifetime"], speedst["speed"]), FUN=mean)
print(speedst.mean)

g <- ggplot(subset(speedst, doPull == "true"), aes(x=speed, y=Loss, group=strategy)) +
     geom_line(aes(color=strategy)) +
     scale_y_continuous ("Loss rate", limits = c(0, NA)) +
     scale_x_discrete ("Speed, m/s") +
     facet_grid(traceLifetime~., scales = "free")
    # + theme_custom ()

if (!file.exists ("graphs/pdfs")) {
  dir.create ("graphs/pdfs")
}

pdf (outputLoss, width=5, height=3.5)
g
x = dev.off ()
