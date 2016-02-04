namespace utils{
    int extractTag(Addr addr){
        return addr >> 20;
    }

    int extractIndex(Addr addr){
        return (addr >> 6) % (1 << 14);
    }
}
