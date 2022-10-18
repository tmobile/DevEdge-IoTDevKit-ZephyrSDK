static const unsigned char CA_CERT[] = {
    // Write results of 'cat AmazonRootCA1.pem | sed -e '1d;$d' | base64 -di - | xxd -i' here
};
 
static const unsigned char DEV_CERT[] = {
   // Write results of 'cat *-certificate.pem.crt | sed -e '1d;$d' | base64 -di - | xxd -i' here
};
 
static const unsigned char DEV_KEY[] = {
   // Write results of 'cat *-private.pem.key | sed -e '1d;$d' | base64 -di - | xxd -i' here
};