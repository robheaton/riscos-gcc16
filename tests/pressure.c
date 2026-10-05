/* leaf function, high register pressure: any callee-saved reg used MUST be saved in the prologue */
int pressure(int *p) {
  int a=p[0],b=p[1],c=p[2],d=p[3],e=p[4],f=p[5],g=p[6],h=p[7],i=p[8],j=p[9];
  return (a*b)+(c*d)+(e*f)+(g*h)+(i*j) + a+b+c+d+e+f+g+h+i+j + a*j + b*i + c*h + d*g + e*f;
}
