/* 
 * dnssec.c
 *
 * contains the cryptographic function needed for DNSSEC
 * The crypto library used is openssl
 *
 * (c) NLnet Labs, 2004
 * a Net::DNS like library for C
 *
 * See the file LICENSE for the license
 */

#include "common.h"
#include <assert.h>

#include <ldns/config.h>
#include <ldns/ldns.h>

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/bn.h>
#include <openssl/bio.h>
#include <openssl/evp.h>

/**
 * Verify an rrsig with the DSA algorithm, see RFC 2536
 */
int
verify_rrsig_dsa(uint8_t *verifybuf, unsigned long length, unsigned char *sigbuf, unsigned int siglen,
		unsigned char *key_bytes, unsigned int keylen)
{
	uint8_t T = (uint8_t) key_bytes[0];
	int numberlength;
	int offset = 1;

	BIGNUM *Q;
	BIGNUM *P;
	BIGNUM *G;
	BIGNUM *Y;
	uint8_t t_sig;

	BIGNUM *R;
	BIGNUM *S;
	DSA *dsa;
	uint8_t *hash;
	DSA_SIG *sig;
	int dsa_res;
	int result;
	

	numberlength = (int) (64 + T * 8);
	
	if (T > 8) {
		warning("DSA type > 8 not implemented, unable to verify signature");
		return RET_FAIL;
	}
	
	Q = BN_bin2bn(&key_bytes[offset], 20, NULL);
	offset += 20;
	
	P = BN_bin2bn(&key_bytes[offset], numberlength, NULL);
	offset += numberlength;
	
	G = BN_bin2bn(&key_bytes[offset], numberlength, NULL);
	offset += numberlength;
	
	Y = BN_bin2bn(&key_bytes[offset], numberlength, NULL);
	offset += numberlength;
	
	t_sig = (uint8_t) sigbuf[0];
	
	if (t_sig != T) {
		warning("Values for T are different in key and signature, verification of DSA sig failed");
		return RET_FAIL;
	}
	
	
	R = BN_bin2bn(&sigbuf[1], 20, NULL);
	
	S = BN_bin2bn(&sigbuf[21], 20, NULL);
	
	dsa = DSA_new();
	
	dsa->p = P;
	dsa->q = Q;
	dsa->g = G;
	dsa->pub_key = Y;
	
	//hash = xmalloc(20);
	
	hash = SHA1((unsigned char *) verifybuf, length, NULL);
	
	sig = DSA_SIG_new();
	sig->r = R;
	sig->s = S;
	
	dsa_res = DSA_do_verify((unsigned char *) hash, 20, sig, dsa);

	if (dsa_res == 1) {
		result = RET_SUC;
	} else if (dsa_res == 0) {
		result = RET_FAIL;
	} else {
		warning("internal error when verifying: %d", dsa_res);
		ERR_print_errors_fp(stdout);
		result = RET_FAIL;
	}

	return result;
}



/**
 * Verify an rrsig with the RSA algorithm and SHA1 hash, see RFC 3110
 */
int
verify_rrsig_rsasha1(uint8_t *verifybuf, unsigned long length, unsigned char *sigbuf, unsigned int siglen,
		unsigned char *key_bytes, unsigned int keylen)
{
	BIGNUM *modulus;
	BIGNUM *exponent;
	unsigned char *modulus_bytes;
	unsigned char *exponent_bytes;
	unsigned char *digest;
	int offset;
	int explength;
	uint16_t int16;
	RSA *rsa;
	int rsa_res;

	int result;
	
	if(key_bytes[0] == 0) {
		memcpy(&int16, &key_bytes[1], 2);
		int16 = ntohs(int16);
		explength = (int) int16;
		offset = 3;
	} else {
		explength = (int) key_bytes[0];
		offset = 1;
	}
	
	/* Exponent */
	exponent_bytes = xmalloc(explength*sizeof(char));
	memcpy(exponent_bytes, &key_bytes[offset], explength*sizeof(char));
	exponent = BN_new();
	(void) BN_bin2bn(exponent_bytes, explength, exponent);
	offset += explength;

	/* Modulus */
	modulus_bytes = xmalloc((keylen-offset)*sizeof(char));
	memcpy(modulus_bytes, &key_bytes[offset], (keylen-offset)*sizeof(char));
	modulus = BN_new();
	(void) BN_bin2bn(&modulus_bytes[0], (int) keylen-offset, modulus);
	offset = (int) keylen;

	rsa = RSA_new();
	rsa->n = modulus;
	rsa->e = exponent;

	digest = SHA1((unsigned char *) verifybuf, length, NULL);
	if (digest == NULL) {
		error("Error digesting");
		exit(EXIT_FAILURE);
	}
	
	rsa_res = RSA_verify(NID_sha1,  digest, 20,  sigbuf, siglen, rsa);
	if (rsa_res == 1) {
		result = RET_SUC;
	} else if (rsa_res == 0) {
		result = RET_FAIL;
	} else {
		warning("internal error when verifying: %d\n", rsa_res);
		ERR_print_errors_fp(stdout);
		result = RET_FAIL;
	}
	
	xfree(modulus_bytes);
	xfree(exponent_bytes);
	RSA_free(rsa);
	return result;
}

int
verify_rrsig_rsamd5(uint8_t *verifybuf, unsigned long length, unsigned char *sigbuf, unsigned int siglen,
		unsigned char *key_bytes, unsigned int keylen)
{
	BIGNUM *modulus;
	BIGNUM *exponent;
	unsigned char *modulus_bytes;
	unsigned char *exponent_bytes;
	int offset, explength;
	unsigned char *digest;
	uint16_t int16;
	RSA *rsa;
	int rsa_res;

	int result;
	
	if(key_bytes[0] == 0) {
		memcpy(&int16, &key_bytes[1], 2);
		int16 = ntohs(int16);
		explength = (int) int16;
		offset = 3;
	} else {
		explength = (int) key_bytes[0];
		offset = 1;
	}
	
	/* Exponent */
	exponent_bytes = xmalloc(explength*sizeof(char));
	memcpy(exponent_bytes, &key_bytes[offset], explength*sizeof(char));
	exponent = BN_new();
	(void) BN_bin2bn(exponent_bytes, explength, exponent);
	offset += explength;

	/* Modulus */
	modulus_bytes = xmalloc((keylen-offset)*sizeof(char));
	memcpy(modulus_bytes, &key_bytes[offset], (keylen-offset)*sizeof(char));
	modulus = BN_new();
	(void) BN_bin2bn(&modulus_bytes[0], (int) keylen-offset, modulus);
	offset = (int) keylen;

	rsa = RSA_new();
	rsa->n = modulus;
	rsa->e = exponent;

	digest = xmalloc(16);
	digest =  MD5((unsigned char *) verifybuf, length,  digest);
	if (digest == NULL) {
		error("Error digesting\n");
		exit(EXIT_FAILURE);
	}
	
	rsa_res = RSA_verify(NID_md5, digest, 16,  sigbuf, siglen, rsa);
	if (rsa_res == 1) {
		result = RET_SUC;
	} else if (rsa_res == 0) {
		result = RET_FAIL;
	} else {
		warning("internal error when verifying: %d\n", rsa_res);
		ERR_print_errors_fp(stdout);
		result = RET_FAIL;
	}
	
	xfree(digest);
	xfree(modulus_bytes);
	xfree(exponent_bytes);
	RSA_free(rsa);
	return result;
}

/**
 * Verifies the rrsig of the rrset with the dnskey
 * What does result?
 */
int
verify_rrsig(struct t_rr *rrset, struct t_rr *rrsig, struct t_rr *dnskey)
{
	/* translate rrsig+rrset to binary data */
	uint8_t *verifybuf;
	unsigned char *sigbuf;
	unsigned char *key_bytes;
	uint32_t int32;
	unsigned long length = 0;
	unsigned int siglen;
	unsigned int keylen;
	int result;
	
	verifybuf = xmalloc(MAX_PACKET);
	sigbuf = (unsigned char *) base64_decode((unsigned char *) rrsig->rdata[8]->data,
		(int) rrsig->rdata[8]->length, (size_t *) &siglen);

	length += sig2verifybytes(rrsig, verifybuf, length, MAX_PACKET);
	rrset_sort(&rrset);

	/* set the ttl in the rrset... */
	int32 = rdata2uint32(rrsig->rdata[3]);
	rrset_set_ttl(rrset, int32);
	length += rrset2wire(rrset, verifybuf, length, MAX_PACKET);
	key_bytes = (unsigned char *) base64_decode((unsigned char *) dnskey->rdata[3]->data,
		(int) dnskey->rdata[3]->length, (size_t *) &keylen);

	if (keylen < 0) {
		warning("Error in base64 decode of key data:");
		/* XXX TODO */
		print_rd(dnskey->rdata[3]);
		printf("\n");
		return RET_FAIL;
	}
	switch (rdata2uint8(rrsig->rdata[1])) {
		case ALG_DSA:
			result = verify_rrsig_dsa(verifybuf, length, sigbuf,
					siglen, key_bytes, keylen);
			break;
		case ALG_RSASHA1:
			result = verify_rrsig_rsasha1(verifybuf, length, sigbuf,
					siglen,	key_bytes, keylen);
			break;
		case ALG_RSAMD5:
			result = verify_rrsig_rsamd5(verifybuf, length, sigbuf,
					siglen, key_bytes, keylen);
			break;
		default:
			warning("unknown or unimplemented algorithm (alg %s nr %d)", namebyint(rdata2uint8(rrsig->rdata[1]), dnssec_algos), rdata2uint8(rrsig->rdata[1]));
print_rr(rrsig, FOLLOW);
			exit(EXIT_FAILURE);
			break;
	}

	xfree(key_bytes);
	xfree(verifybuf);
	xfree(sigbuf);
	
	return result;
}
