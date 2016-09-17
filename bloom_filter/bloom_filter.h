/*************************************************************
 > File Name	: bloom_filter.h
 > Describe     : The Standard Bloom Filter
 > Author		: zhongsx
 > Mail 		: kuai2361425@163.com
 > Time:		: 2016-09-16 21:20:22
*************************************************************/

#ifndef __BLOOM_FILTER_H__
#define __BLOOM_FILTER_H__

#define BLOOM_FILTER_ITEM_MAXN_MICRO                 (1 << 16)
#define BLOOM_FILTER_ITEM_MAXN_SMALL                 (1 << 20)
#define BLOOM_FILTER_ITEM_MAXN_LARGE                 (1 << 24)
#define BLOOM_FILTER_BYTE_MAXN_LARGE                 (1 << 28)

#include <atomic>
#include <iostream>
#include <stdint.h>
#include <math.h>
#include <assert.h>

/*! the bloom filter type
 *
 * A Bloom filter is a space-efficient probabilistic data structure, 
 * conceived by Burton Howard Bloom in 1970, that is used to test whether an element is a member of a set. 
 * False positive matches are possible, but false negatives are not; 
 * i.e. a query returns either "possibly in set" or "definitely not in set". 
 * Elements can be added to the set, but not removed (though this can be addressed with a "counting" filter). 
 * The more elements that are added to the set, the larger the probability of false positives.
 *
 * Assume that a hash function selects each array position with equal probability. 
 * If m is the number of bits in the array, and k is the number of hash functions, 
 * then the probability that a certain bit is not set to 1 by a certain hash function 
 * during the insertion of an element is then
 * 1 - 1 / m
 *
 * The probability that it is not set to 1 by any of the hash functions is
 * (1 - 1/ m) ^ k
 *
 * If we have inserted n elements, the probability that a certain bit is still 0 is
 * (1 - 1/ m) ^ kn
 *
 * the probability that it is 1 is therefore
 * 1 - ((1 - 1/ m) ^ kn)
 *
 * Now test membership of an element that is not in the set.
 * Each of the k array positions computed by the hash functions is 1 with a probability as above.
 * The probability of all of them being 1, 
 * which would cause the algorithm to erroneously claim that the element is in the set, is often given as
 * p = (1 - ((1 - 1/ m) ^ kn))^k ~= (1 - e^(-kn/m))^k
 *
 * For a given m and n, the value of k (the number of hash functions) that minimizes the probability is
 * k = (m / n) * ln2 ~= (m / n) * (9 / 13)
 *
 * which gives
 * 2 ^ -k ~= 0.6185 ^ (m / n)
 *
 * The required number of bits m, given n (the number of inserted elements) 
 * and a desired false positive probability p (and assuming the optimal value of k is used) 
 * can be computed by substituting the optimal value of k in the probability expression above:
 * p = (1 - e ^-(m/nln2)n/m))^(m/nln2)
 *
 * which can be simplified to:
 * lnp = -m/n * (ln2)^2
 *
 * This optimal results in:
 * s = m/n = -lnp / (ln2 * ln2) = -log2(p) / ln2
 * k = s * ln2 = -log2(p) <= note: this k will be larger
 *
 * compute s(m/n) for given k and p:
 * p = (1 - e^(-kn/m))^k = (1 - e^(-k/s))^k
 * => lnp = k * ln(1 - e^(-k/s))
 * => (lnp) / k = ln(1 - e^(-k/s))
 * => e^((lnp) / k) = 1 - e^(-k/s)
 * => e^(-k/s) = 1 - e^((lnp) / k) = 1 - (e^lnp)^(1/k) = 1 - p^(1/k)
 * => -k/s = ln(1 - p^(1/k))
 * => s = -k / ln(1 - p^(1/k)) and define c = p^(1/k)
 * => s = -k / ln(1 - c)) and ln(1 + x) ~= x - 0.5x^2 while x < 1 
 * => s ~= -k / (-c-0.5c^2) = 2k / (2c + c * c)
 *
 * so 
 * c = p^(1/k)
 * s = m / n = 2k / (2c + c * c)
 */

/// the probability of false positives
typedef enum _TYPE_PROBABILITY
{
    BLOOM_FILTER_PROBABILITY_0_1         = 3, ///!< 1 / 2^3 = 0.125 ~= 0.1
    BLOOM_FILTER_PROBABILITY_0_01        = 6, ///!< 1 / 2^6 = 0.015625 ~= 0.01
    BLOOM_FILTER_PROBABILITY_0_001       = 10, ///!< 1 / 2^10 = 0.0009765625 ~= 0.001
    BLOOM_FILTER_PROBABILITY_0_0001      = 13, ///!< 1 / 2^13 = 0.0001220703125 ~= 0.0001
    BLOOM_FILTER_PROBABILITY_0_00001     = 16, ///!< 1 / 2^16 = 0.0000152587890625 ~= 0.00001
    BLOOM_FILTER_PROBABILITY_0_000001    = 20, ///!< 1 / 2^20 = 0.00000095367431640625 ~= 0.000001        
}TYPE_PROBABILITY;


class bloom_filter
{
public:
    bloom_filter()
    {
        bits = NULL;
    }

    ~bloom_filter()
    {

    }
    /*
     * pro  == The probablity of false positives
     * nmax == The max of item
     * hnum == The number of hash func
     * return :
     *  0 error
     *  1 sucess
     */
    int bloom_filter_init(TYPE_PROBABILITY pro, uint64_t nmax, int hash)
    {
        if (bits){
            return 0;
        }

        if (nmax > BLOOM_FILTER_ITEM_MAXN_LARGE){
            return 0;
        }
        
        /* compute the storage space
         * c = p ^ ( 1/k)
         * s = m / n = 2k / (2c + c*c)
         */
#ifdef COMPUTE_THE_STORAGE_SPACE
        double k = (double)hash;
        double p = 1 / (double)(1 << pro);
        double c = pow(p, 1 / k);
        double s = (k + k) / ( c + c + c * c);
        uint64_t m = ceil(s * nmax);
#else 
    #if 1
        //make scale table
        int i = 0;
        for (i = 1; i < 16; i++)
        {
            printf("\t{");
            for (int j = 0; j < 31; j++)
            {
                double k = (double)i;
                double p = 1 / (double)(1 << i);
                double c = pow(p, 1 / k);
                double s = (k + k) / ( c + c + c * c);
                if (j != 30) printf("%lf", s);
                else printf("%lf },\n", s);
            }
            
        }
    #endif 
        //the scale
        static double s_scale[15][31]={
            //......
        };

        //m = (s * n);
        uint64_t m = ceil(s_scale[hnum-1][pro] * nmax);
#endif 
        
        //init 
        hnum = hash;
        uhashmax = m;
        uitemmax = nmax;
        
        usize = (m >> 3) + 1;
        if (usize > BLOOM_FILTER_BYTE_MAXN_LARGE){
            return 0;
        }

        bits = new uint8_t[usize];
        assert(bits != NULL);

        return true;
    }

    int bloom_filter_set(const char* str, int len)
    {
        if (ucount >= uitemmax){
            return 0;
        }
       
        ucount++; //atomic

        uint64_t hash_key1 = BKDRHash(str, len);
        uint64_t hash_key2 = DJBHash(str, len);

        for(int i = 0; i < hnum; i++)
        {
            uint64_t hash_value = (hash_key1 + i * hash_key2)%uhashmax;
            bloom_filter_set_bit(hash_value);
        }
        
        return 1;
    }

    int bloom_filter_get(const char* str, int len)
    {
        
        uint64_t hash_key1 = BKDRHash(str, len);
        uint64_t hash_key2 = DJBHash(str, len);
        
        int n = 0;
        for(n = 0; n < hnum; n++)
        {
            uint64_t hash_value = (hash_key1 + n * hash_key2)%uhashmax;
            if (!bloom_filter_get_bit(hash_value)){
                break;
            }
        }

        return (n == hnum) ? 1 : 0 ;
    }
    
protected:
    uint64_t BKDRHash(const char* str, int len)
    {
        uint64_t hash =0;
        uint64_t seed = 131;
        for(int i = 0; i < len; ++i){
            hash += hash * seed + *(str+i); 
        }

        return hash;
    }
    
    uint64_t DJBHash(const char* str, int len)
    {
        uint64_t hash = 5381;
        while(len--)
            hash += (hash << 5) + (*str++);
        return hash;
    }

    void bloom_filter_set_bit(uint64_t index)
    {
        bits[index >> 3] |= (0x1 << (index & 7));
    }

    int bloom_filter_get_bit(uint64_t index)
    {
        return  bits[index >> 3] & (0x1 << (index & 7));
    }

private:
    uint8_t* bits;  //save pos 
    std::atomic<uint64_t> ucount; //number of item 
    uint64_t usize; //the max number of bits;
    uint64_t uhashmax; //size to bits memory ~= uhashmax >> 3

    uint64_t uitemmax; // the max number of item

    int     hnum; //nubmer of hash func

};



#endif //__BLOOM_FILTER_H__
