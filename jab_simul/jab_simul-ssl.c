
#include "jab_simul-main.h"

//#ifdef HAVE_SSL

#include <openssl/err.h>
#include <openssl/ssl.h>


SSL_CTX *ssl__ctx;

void jab_ssl_stop() {

  if (ssl__ctx)
    SSL_CTX_free(ssl__ctx);		
  
  ERR_free_strings();
  EVP_cleanup();
}

int jab_ssl_init()
{
    SSL_CTX *ctx = NULL;

    /* Generic SSL Inits */
    OpenSSL_add_all_algorithms();    
    SSL_load_error_strings();

    ctx=SSL_CTX_new(SSLv23_client_method());
    if(ctx == NULL)  {
      unsigned long e;
      static char *buf;
      
      e = ERR_get_error();
      buf = ERR_error_string(e, NULL);
      fprintf(stderr, "Could not create SSL Context: %s\n", buf);
      return 1;
    }
	
   
    ssl__ctx =  ctx;
    
    return 0;
}

ssize_t jab_ssl_read(void *ssl, void *buf, size_t count, int *blocked)
{
    int count_ret;
    ssize_t ret;
    int sret;

   
    *blocked = 0;

    if(count <= 0)
        return 0;

    if(SSL_get_state(ssl) != TLS_ST_OK)
      {
      sret = SSL_connect(ssl);
        if(sret <= 0)
        {
            unsigned long e;
            static char *buf;
            
            if((SSL_get_error(ssl, sret) == SSL_ERROR_WANT_READ) ||
               SSL_get_error(ssl, sret) == SSL_ERROR_WANT_WRITE)
            {
		*blocked = 1;
                return -1;
            }
            e = ERR_get_error();
            buf = ERR_error_string(e, NULL);
            fprintf(stderr, "Error read1 from SSL: %s\n", buf);
            return -1;
        }       
    }

    count_ret = 0;
    do {
      ret = SSL_read(ssl, (char *)buf, count);	  
      if (ret > 0) {
	count_ret += ret;
	buf += ret;
	count -= ret;
	  }
    } while (count>0 && ret>0);
    
    if ((count_ret==0)&&(ret==-1)) {
      int err;
      
      count_ret = -1;
      
      err = SSL_get_error(ssl, ret);
      
      if ((err == SSL_ERROR_WANT_READ) ||
	  (err == SSL_ERROR_WANT_WRITE)) {	
	*blocked = 1;
	return -1;
      }
	  
      if (err == 1) {
	char buf[121];
	while ((err = ERR_get_error())>0) {
	  ERR_error_string(err,buf);
	  fprintf(stderr,"errer read2 %d %s\n",err,buf);
	}
      }
    }
    
    return count_ret;
}

ssize_t jab_ssl_write(void *ssl, const void *buf, size_t count, int *blocked)
{
    *blocked = 0;

    if(SSL_get_state(ssl) != TLS_ST_OK)
    {
        int sret;

        sret = SSL_connect(ssl);
        if(sret <= 0){
            unsigned long e;
            static char *buf;
            
            if((SSL_get_error(ssl, sret) == SSL_ERROR_WANT_READ) ||
               SSL_get_error(ssl, sret) == SSL_ERROR_WANT_WRITE)
            {
               
		*blocked = 1;
                return -1;
            }
            e = ERR_get_error();
            buf = ERR_error_string(e, NULL);
            fprintf(stderr, "Error write from SSL: %s\n", buf);
            return -1;
        }       
    }

    return SSL_write(ssl, buf, count);    
}

void *jab_ssl_conn(int fd) {
  void *ssl;

  if (!ssl__ctx) return NULL;

  ssl = SSL_new(ssl__ctx);
  if (!ssl) return NULL;

  SSL_set_fd(ssl, fd);
  SSL_set_connect_state(ssl);

  return ssl;
}

int jab_ssl_connect(void *ssl) {
  int sret;

  sret = SSL_connect(ssl);
  if(sret <= 0)   {
    unsigned long e;
    static char *buf;
    
    if((SSL_get_error(ssl, sret) == SSL_ERROR_WANT_READ) ||
       (SSL_get_error(ssl, sret) == SSL_ERROR_WANT_WRITE))
      {
	return 0;
      }
    e = ERR_get_error();
    buf = ERR_error_string(e, NULL);
    fprintf(stderr, "Error connect from SSL: %s\n", buf);
    return -1;
  }
  return 1;
}


void jab_free_conn(void *ssl) {
  if (ssl)
    SSL_free(ssl);
}

//#endif /* HAVE_SSL */



