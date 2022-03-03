#!/usr/bin/env python3
import sys, os
import traceback
import gzip
import argparse
from collections import defaultdict
import subprocess
import re

mydir = os.path.split(__file__)
sys.path.append(mydir)
from misc import *
import prjfile as prj


def fx_sg2csv(argv):
    '''将string graph转换成gephi可识别的csv文件（边）'''
    parser = argparse.ArgumentParser(fx_sg2csv.__doc__)
    parser.add_argument("sg", type=str)
    parser.add_argument("csv", type=str)
    parser.add_argument("--type", type=str, default="active")
    parser.add_argument("--single", action="store_true")

    try:
        args = parser.parse_args(argv)

        csv = open(args.csv, "w", encoding="utf8")
        csv.write("source, target, score, identity\n")

        nodes = set()
        for line in open_file(args.sg):
            its = line.split()
            if args.type == "" or its[-1] in args.type:
                csv.write("%s, %s, %s, %s\n" % (its[0], its[1], its[5], its[6]))
                nodes.add(its[0][:-1])
                nodes.add(its[1][:-1])
                    
        print(args.single)
        if args.single:
            for n in nodes:
                csv.write("%s, %s, 0, 0\n" % (n+"B", n+"E"))
                csv.write("%s, %s, 0, 0\n" % (n+"E", n+"B"))

    except:
        traceback.print_exc()
        print("-----------------")
        parser.print_usage()

def fx_sgsub(argv):
    '''以某点为中心，提取部分string graph'''
    parser = argparse.ArgumentParser(fx_sgsub.__doc__)
    parser.add_argument("centre", type=str)
    parser.add_argument("--graph", type=str, default="graph_edges.gz")
    parser.add_argument("--output", type=str, default="")
    parser.add_argument("--types", type=str, default="active,type1,phased,bridged")
    parser.add_argument("--radius", type=int, default=30)

    
    try:
        args = parser.parse_args(argv)
        sgname, csvname, centre, radius, types = args.graph, args.output, args.centre, args.radius, args.types

        if not centre.endswith(":E") and not centre.endswith(":B"):
            centre += ":E"

        if csvname == "":
            csvname = centre.split(":")[0] + ".csv"
        
        csv = open(csvname, "w", encoding="utf8")

        radius = int(radius)

        items = set()
        group = set()
        names = set()

        for line in open_file(sgname):
            its = tuple(line.split())
            if types=="" or its[-1] in types:
                items.add(its)

        names.add(centre)

        for i in range(radius):
            for its in items:
                if its[0] in names or its[1] in names:
                    group.add(its)
                    names.add(its[0])
                    names.add(its[1])


        csv.write("source, target, score, identity, centre\n")
        for its in group:
            ccc = 1 if its[0] == centre or  its[1] == centre else 0
            csv.write("%s, %s, %s, %s, %d\n" % (its[0], its[1], its[5], its[6], ccc))

    except:
        traceback.print_exc()
        print("-----------------")
        print(parser.description)

def fx_sgctg(argv):
    parser = argparse.ArgumentParser("生成contig信息")
    parser.add_argument("tiles", type=str)
    parser.add_argument("csv", type=str)
    try:        
        args = parser.parse_args(argv)

        reads = {}
        for line in open(args.tiles):
            its = line.split()
            nodes = its[1].split("=")[1].split("~")
            reads[nodes[0][:-1]+"B"] = its[0]
            reads[nodes[0][:-1]+"E"] = its[0]
            reads[nodes[1][:-1]+"B"] = its[0]
            reads[nodes[1][:-1]+"E"] = its[0]
            
        with open(args.csv, "w") as ofile:
            ofile.write("Id, contig\n")

            for n, c in reads.items():
                ofile.write("%s,%s\n" % (n, c))
        pass
    except:
        traceback.print_exc()
        print("-----------------")
        print(parser.description)



def fx_ctginfo(argv):
    parser = argparse.ArgumentParser("查询ctg头尾节点信息")
    parser.add_argument("ctg", type=str)
    parser.add_argument("--wrkdir", type=str, default=".")
    try:        
        import itertools

        args = parser.parse_args(argv)
        name = args.ctg
        fname = [args.wrkdir + "/primary_tiles", args.wrkdir + "/alternate_tiles"]

        tiles = []
        for line in itertools.chain(open(fname[0]), open(fname[1])):
            its = line.split()
            if (its[0] == name):
                tiles.append(its)


        print("line:", len(tiles))
        print("head:", tiles[0])
        head = tiles[0][1].split("=")[1].split("~")[0][:-2]
        os.system("grep -w %s %s %s" % (head, fname[0], fname[1]))
        print("tail:", tiles[-1])
        head = tiles[-1][1].split("=")[1].split("~")[1][:-2]
        os.system("grep -w %s %s %s" % (head, fname[0], fname[1]))

        pass
    except:
        traceback.print_exc()
        print("-----------------")
        print(fx_ctginfo.__doc__)




def fx_id2name(argv):
    parser = argparse.ArgumentParser("获得id的名称")
    parser.add_argument("--id2name", type=str, default="id2name.txt.gz")
    parser.add_argument("--reverse", action="store_true")
    parser.add_argument("ids", type=str, nargs="+")

    try:
        args = parser.parse_args(argv)
        id2name = load_id_name(args.id2name, args.reverse)
        for i in args.ids:
            print(i, id2name[i])


    except:
        traceback.print_exc()
        print("-----------------")
        parser.print_usage()


def fx_getseq(argv):
    '''获取部分reads'''
    parser = argparse.ArgumentParser("根据名称从fastq文件里读取seq")
    parser.add_argument("--reads", type=str, default="../1-correct/corrected_reads.fasta")
    parser.add_argument("--prefix", type=str, default="./sub/")
    parser.add_argument("--id2name", type=str, default="")
    parser.add_argument("names", type=str, nargs="+")

    try:
        args = parser.parse_args(argv)

        
        if len(args.id2name) > 0: 
            id_name = load_id_name("./id2name.txt.gz")
            names = [id_name[i] for i in args.names]
            for i, n in zip(args.names, names):
                print(i, n)
        else:
            names = args.names
        faipath = args.reads + ".fai"
        run_if_modified([args.reads], [faipath], "samtools faidx %s" % args.reads)

        cmds = ""
        for name in names:
            cmd = 'samtools faidx %s %s > %s%s.fasta' % (args.reads, name, args.prefix, name)
            cmds += cmd + " & "
        cmds += " wait "
        print(cmds)
        os.system(cmds)
    except:
        traceback.print_exc()
        print("-----------------")
        parser.print_usage()

def fx_snp_info(argv):
    parser = argparse.ArgumentParser("get snp in reads")
    parser.add_argument("--infos", type=str, default="")
    parser.add_argument("names", type=str, nargs="+")

    try:
        args = parser.parse_args(argv)
        if args.infos == "":
            args.infos = os.path.join(prj.find_prjpath("4-phase"), "readinfos")


        cond = "||".join([ "$2==%s" % n for n in args.names])
        cmd = "awk '{if (%s) { print $0 }}' %s " % (cond, args.infos)
        logger.info("Command:%s" % cmd)
        os.system(cmd)
    except:
        traceback.print_exc()
        print("-----------------")
        parser.print_usage()


def fx_getol(argv):
    '''获取部分reads'''
    try:
        name = argv[0]
        olfile = "../2-align/overlaps.paf"
        cmd = """awk '{ if ($1==%s || $6==%s) { print $0 }}' %s > sub/%s.paf""" % (name, name, olfile, name)
        print(cmd)
        os.system(cmd)
    except:
        traceback.print_exc()
        print("-----------------")
        print(fx_getseq.__doc__)


def get_contig_reads2(tile, id2name):

    m = {}
    for line in open_file(id2name):
        its = line.split()
        m[its[0]] = its[1]

    ctgs = defaultdict(list)

    for line in open_file(tile):
        its = line.split()
        s = ctgs[its[0]]
        r = its[1].split("=")[1].split("~")
        if len(s) == 0:
            a = str(int(r[0].split(":")[0]))
            s.append(m[a])
        a = str(int(r[1].split(":")[0]))
        s.append(m[a])

    return ctgs

 

def fx_read_in_contig(argv):
    parser = argparse.ArgumentParser("找到contigs中的reads")
    parser.add_argument("ctg", type=str)
    parser.add_argument("tile", type=str)
    parser.add_argument("--binfos", type=str)


    try:
        args = parser.parse_args(argv)
        from collections import defaultdict

        logger.info("Loading reads in contigs")
        ctgs = PrjFile.get_contig_reads(args.tile)

        binfos = None
        if args.binfos:
            binfos = prj.BinInfos(args.binfos)

        for c in ctgs[args.ctg]:
            print("%s %s" % (c, binfos.infos[c]), binfos.kmers[c])

    except:
        traceback.print_exc()
        print("-----------------")
        parser.print_usage()



def fx_filter_paf_with_bininfos(argv):
    '''使用bin信息过滤paf文件'''
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument("ifname", type=str)
    parser.add_argument("ofname", type=str)
    parser.add_argument("bininfos", type=str)

    try:
        args = parser.parse_args(argv)

        logger.info("加载bininfos（fsa_kmer_bin产生）信息")

        classified = [set(), set(), set()]
        for line in open(args.bininfos):
            its = line.split()      # name ... type[1|0|-1]
            classified[int(its[-1])+1].add(its[0])

        logger.info("过滤")
        with open(args.ofname, "w") as ofile:
            for i, line in enumerate(open(args.ifname)):
                its = line.split()
                s0, s1 = its[0], its[5]
                if s0 in classified[0] and s1 in classified[2] or s0 in classified[2] and s1 in classified[0]:
                    pass
                else:
                    ofile.write(line)

                if i % 1000000 == 0:
                    print(i)
    except:
        traceback.print_exc()
        print("-----------------")
        parser.print_usage()

def detect_overlap_name_postion(fname):
    if fname.endswith(".paf") or fname.endswith(".paf.gz"):
        return (0, 5)
    elif fname.endswith(".m4") or fname.endswith(".m4.gz"):
        return (0, 1)
    elif fname.endswith(".m4a") or fname.endswith(".m4a.gz"):
        return (0, 1)
    else:
        assert 0, "Failed to recognize overlap format"

def fx_eval_overlap_haplotype(argv):
    parser = argparse.ArgumentParser("使用reads的分类信息（fsa_kmer_bin生成），评估overlaps中有多少单倍型混合")
    parser.add_argument("overlaps", type=str)
    parser.add_argument("binfos", type=str)

    try:
        args = parser.parse_args(argv)

        logger.info("加载binfos（fsa_kmer_bin产生）信息")
        binfos = prj.BinInfos(args.binfos)

        logger.info("检查 overlaps")
        pos = detect_overlap_name_postion(args.overlaps)
        count = [0, 0, 0]
        for line in open_file(args.overlaps):
            its = line.split()
            r0, r1 = its[pos[0]], its[pos[1]]
            count[1+binfos.test(r0, r1)] += 1

        print(count, 1 - count[0]/sum(count))

    except:
        traceback.print_exc()
        print("-----------------")
        parser.print_usage()

def fx_eval_phased(argv):
    parser = argparse.ArgumentParser("使用reads的分类信息（fsa_kmer_bin生成），检查phased reads的正确率")
    parser.add_argument("phased", type=str)
    parser.add_argument("binfos", type=str)

    try:
        args = parser.parse_args(argv)

        logger.info("加载binfos（fsa_kmer_bin产生）信息")

        classified = [set(), set(), set()]
        for line in open(args.binfos):
            its = line.split()      # name ... type[1|0|-1]
            classified[int(its[-1])+1].add(its[0])

        logger.info("加载phased信息")
        count = [0, 0]
        phased = defaultdict(set)
        for line in open(args.phased):
            its = line.strip().split(':')
            set0 = its[0].split(',')
            set1 = [i.split("|")[0] for i in its[1].split(',')[:-1]]
            # assert len(set0) == 1
            for s0 in set0:
                for s1 in set1:
                    count[0] += 1
                    if s0 in classified[0] and s1 in classified[2] or s0 in classified[2] and s1 in classified[0]:
                        count[1] += 1
                    else:
                        print(s0, s1)
                        pass

        print(count, count[1]/count[0])

    except:
        traceback.print_exc()
        print("-----------------")
        parser.print_usage()

def fx_compare_binfos(argv):
    parser = argparse.ArgumentParser("比较两个BIN的信息")
    parser.add_argument("binfos0", type=str)
    parser.add_argument("binfos1", type=str)
    parser.add_argument("--method", type=str, default="")

    try:
        args = parser.parse_args(argv)
        binfos0 = BinInfos(args.binfos0)
        binfos1 = BinInfos(args.binfos1)

        if args.method == "":
            count = [0, 0]
            for n, v in binfos1.infos.items():
                #if (v * binfos0.infos[n] == -1):
                if (v != binfos0.infos[n]):

                    count[1] += 1
                count[0] += 1

            print(count, 1-count[1]/count[0])
        elif args.method == "read":
            for n, v0 in binfos0.kmers.items():
                if n in binfos1.kmers:
                    v1 = binfos1.kmers[n]
                    x0 = v0[0] / sum(v0) if sum(v0) > 0 else 0
                    y0 = v0[1] / sum(v0) if sum(v0) > 0 else 0
                    x1 = v1[0] / sum(v1) if sum(v1) > 0 else 0
                    y1 = v1[1] / sum(v1) if sum(v1) > 0 else 0
                    print(n, x0, y0, x1, y1)
        elif args.method == "read1":
            for n, v0 in binfos0.kmers.items():
                if n in binfos1.kmers:
                    v1 = binfos1.kmers[n]
                    r0 = min(v0) / sum(v0) if sum(v0) > 0 else 0
                    r1 = min(v1) / sum(v1) if sum(v1) > 0 else 0
                    print(n, r0, r1)

    except:
        traceback.print_exc()
        print("-----------------")
        parser.print_usage()

def fx_eval_inconsistent_overlaps(argv):
    parser = argparse.ArgumentParser("比较两个BIN的信息")
    parser.add_argument("binfos", type=str)
    parser.add_argument("--detail", type=str, default="")
    parser.add_argument("overlaps", type=str)

    try:
        args = parser.parse_args(argv)
        binfos = BinInfos(args.binfos)

        pos = [0, 5] if args.overlaps.endswith(".paf") else [0, 1]

        detail = None if len(args.detail) == 0 else open(args.detail, "w")

        count = [0, 0]
        for line in open(args.overlaps):
            its = line.split()
            q, t = its[pos[0]], its[pos[1]]
            if (binfos.infos[q] * binfos.infos[t] == -1):
                count[1] += 1
                if detail != None: detail.write(line)
            count[0] += 1


        print(count, 1-count[1]/count[0])

    except:
        traceback.print_exc()
        print("-----------------")
        parser.print_usage()

def fx_cmp_phased(argv):
    parser = argparse.ArgumentParser("比较两个phased的信息")
    parser.add_argument("phased0", type=str)
    parser.add_argument("phased1", type=str)

    try:
        args = parser.parse_args(argv)

        phased0 = PrjFile.load_phased(args.phased0)
        phased1 = PrjFile.load_phased(args.phased1)

        diff = []
        for k, v0 in phased0.items():
            v1 = phased1[k]
            #sz = (v0 | v1) - (v0 & v1)
            sz = v0 - v1
            diff.append((k, len(sz), sz))

        diff.sort(key=lambda a : -a[1])

        for i, d in enumerate(diff):
            print(d)


    except:
        traceback.print_exc()
        print("-----------------")
        parser.print_usage()

def fx_analyze_snps(argv):
    parser = argparse.ArgumentParser("分析SNP")
    parser.add_argument("variants", type=str)

    try:
        args = parser.parse_args(argv)

        for line in open(args.variants):
            its = line.split()
            name, pos, *vs = its
            vs = [int(v) for v in vs]
            s = sum(vs)
            count = sum([1 for v in vs if v >= 0.2*s])
            if count >= 2:
                print(pos)

    except:
        traceback.print_exc()
        print("-----------------")
        parser.print_usage()

def fx_compare_variants(argv):
    parser = argparse.ArgumentParser("比较两个Variants文件")
    parser.add_argument("vars0", type=str)
    parser.add_argument("vars1", type=str)
    parser.add_argument("--valid0", type=str)
    parser.add_argument("--valid1", type=str)
    parser.add_argument("--common", type=str)

    try:
        args = parser.parse_args(argv)

        def load_snps(fname):
            snps = {}
            for line in open(fname):
                its = line.split()
                if its[-1] == '1':
                    snps[(its[0], int(its[1]))] = [int(i) for i in its[2:-1]]
            return snps

        snps0 = load_snps(args.vars0)
        snps1 = load_snps(args.vars1)

        comm = set(snps0.keys()).intersection(set(snps1.keys()))

        logger.info("snps0 size: %d" % len(snps0))
        logger.info("snps1 size: %d" % len(snps1))
        logger.info("common size: %d" % len(comm))

        def calc_freq(v):
            sv = list(sorted(v))
            return sv[-3] / sv[-1] # secondary / cov

        if args.valid0:
            logger.info("output: %s" % args.valid0)
            ofile = open(args.valid0, "w")
            for k in sorted(snps0.keys()):
                print("_".join([str(ik) for ik in k]), calc_freq(snps0[k]), file=ofile)

        if args.valid1:
            logger.info("output: %s" % args.valid1)
            ofile = open(args.valid1, "w")
            for k in sorted(snps1.keys()):
                print("_".join([str(ik) for ik in k]), calc_freq(snps1[k]), file=ofile)

        if args.common:
            logger.info("output: %s" % args.common)
            ofile = open(args.common, "w")
            for k in comm:
                print("_".join([str(ik) for ik in k]), calc_freq(snps0[k]), calc_freq(snps1[k]), file=ofile)

    except:
        traceback.print_exc()
        print("-----------------")
        parser.print_usage()

def fx_compare_snps_in_reads(argv):
    parser = argparse.ArgumentParser("比较snps在不同reads的分布")
    parser.add_argument("vars0", type=str)
    parser.add_argument("vars1", type=str)
    parser.add_argument("--snps_in_contigs", type=str)

    try:
        args = parser.parse_args(argv)

        def load_snps_in_reads(fname, comm):
            snps = {}
            for line in open(fname):
                its = line.split()
                count = 0
                for i in its[4:]:
                    print(i)
                    p, *_ = i.split("-")
                    if comm == None or (its[1], int(p)) in comm:
                        count += 1
                snps[(its[1], its[3])] = count
            return snps


        def load_comm_snps(fname):
            comm = set()
            for line in open(fname):
                c, p = line.split()[0].split("_")

                comm.add((c, int(p)))
            return comm

        comm_snps = load_comm_snps(args.snps_in_contigs) if args.snps_in_contigs else None

        snps0 = load_snps_in_reads(args.vars0, comm_snps)
        snps1 = load_snps_in_reads(args.vars1, comm_snps)



        comm = set(snps0.keys()).intersection(set(snps1.keys()))

        logger.info("snps0 size: %d" % len(snps0))
        logger.info("snps1 size: %d" % len(snps1))
        logger.info("common size: %d" % len(comm))

        for k in comm:
            print("_".join([str(ik) for ik in k]), snps0[k], snps1[k])


    except:
        traceback.print_exc()
        print("-----------------")
        parser.print_usage()


def fx_compare_phased(argv):
    parser = argparse.ArgumentParser("比较snps在不同reads的分布")
    parser.add_argument("phased0", type=str)
    parser.add_argument("phased1", type=str)

    try:
        args = parser.parse_args(argv)


        phased0 = PrjFile.load_phased(args.phased0)
        phased1 = PrjFile.load_phased(args.phased1)

        for k0, v0 in phased0.items():
            s = set()
            if k0 not in phased1:
                s = v0
            else:
                s = v0 - phased1[k0]

            if len(s) > 0:
                print(k0, s)

    except:
        traceback.print_exc()
        print("-----------------")
        parser.print_usage()



def fx_max_haperr(argv):
    parser = argparse.ArgumentParser("比较两个Variants文件")
    parser.add_argument("binfos", type=str)

    try:
        args = parser.parse_args(argv)
        infos = []
        for line in open(args.binfos):
            its = line.split()
            pkmer, mkmer = int(its[2]), int(its[3])
            infos.append((line, min(pkmer, mkmer)))

        infos.sort(key = lambda x:  -x[1])

        for i, it in enumerate(infos):
            if i > 10: break
            print(it[0], end="")

    except:
        traceback.print_exc()
        print("-----------------")
        parser.print_usage()

def fx_find_switch(argv):
    parser = argparse.ArgumentParser("")
    parser.add_argument("binfos", type=str)
    parser.add_argument("--max_count", type=int, default=10)

    try:
        args = parser.parse_args(argv)
        infos = []
        for line in open(args.binfos):
            its = line.split()
            pkmer, mkmer = int(its[2]), int(its[3])
            infos.append((line, min(pkmer, mkmer)))

        infos.sort(key = lambda x:  -x[1])

        for i, it in enumerate(infos):
            if i > args.max_count: break
            print(it[0], end="")


    except:
        traceback.print_exc()
        print("-----------------")
        parser.print_usage()

def fx_snp_diff(argv):
    parser = argparse.ArgumentParser("两条reads的SNPs的异同。")
    parser.add_argument("rinfos", type=str)
    parser.add_argument("read0", type=str)
    parser.add_argument("read1", type=str)
    parser.add_argument("--w", type=int, default=0)

    try:
        args = parser.parse_args(argv)
        rinfos = PrjFile.load_snp_in_read2(args.rinfos)

        n0, n1 = args.read0, args.read1
        w = args.w
        print(n0, n0 in rinfos)
        print(n1, n1 in rinfos)
        if n0 in rinfos and n1 in rinfos:
            r0 = rinfos[n0]
            r1 = rinfos[n1]

            for ir0 in r0:
                for ir1 in r1:
                    if ir0[0] != ir1[0]: continue
                    count = [0, 0]
                    detail = defaultdict(set)
                    for k, v in ir0[1].items():
                        if k in ir1[1]:
                            if v[w] == "-1" or ir1[1][k][w] == "-1": continue
                            if v[w] == ir1[1][k][w]:
                                count[0] += 1
                            else:
                                count[1] += 1
                            detail[k].add(v[w])
                            detail[k].add(ir1[1][k][w])

                    #print(detail)
                    print(count)
                    print(ir0[0],ir1[0])
                    print(ir0[0])
    except:
        traceback.print_exc()
        print("-----------------")
        parser.print_usage()

def fx_stat_binfo(argv):
    parser = argparse.ArgumentParser("统计binfo信息")
    parser.add_argument("binfo", type=str)
   
    try:
        args = parser.parse_args(argv)
        
        binfo = prj.Binfo(args.binfo)
        stat = binfo.stat()
        
        print("All Kmer: %d(%.04f)\n" % (stat[0], 1))
        print("Err Kmer: %d(%.04f)\n" % (stat[1], stat[1]/stat[0]))
        print("Hap Kmer:  %d(%.04f)\n" % (stat[2], stat[2]/stat[0]))
        print("Hap Err:  %d(%.04f)\n" % (stat[3], stat[3]/stat[2]))
        print("Kmer Err:  %d(%.04f)\n" % ((stat[2]+stat[1]), (stat[2]+stat[1])/stat[0]))


    except:
        traceback.print_exc()
        print("-----------------")
        parser.print_usage()


def fx_tile_breakpoint(argv):
    parser = argparse.ArgumentParser("tiles断开位置")
    parser.add_argument("tile", type=str)
   
    try:
        args = parser.parse_args(argv)
        
        tiles = defaultdict(list)
        for line in open(args.tile):
            its = line.split()
            tiles[its[0]].append(its[1].split("=")[1].split("~"))

        for c, ts in tiles.items():
            for t0, t1 in zip(ts[0:-1], ts[1:]):
                if t0[1] != t1[0]:
                    print(c, t0, t1)


    except:
        traceback.print_exc()
        print("-----------------")
        parser.print_usage()



if __name__ == '__main__':
    script_entry(sys.argv, locals())