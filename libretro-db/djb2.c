unsigned djb2( const char* str, unsigned len )
{
  const unsigned char* aux = (const unsigned char*)str;
  unsigned hash = 5381;
  
  while ( len-- )
  {
    hash = ( hash << 5 ) + hash + *aux++;
  }
  
  return hash;
}
