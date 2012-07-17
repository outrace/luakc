luakc
=====

Another kyotocabinet lua binding<br/>
由于kc作者写的kc binding无法在ngx_lua上使用（因为它用到了全局的table。而在ngx_lua中，当cache_code=on的时候，这是不允许的）<br/>
根据章亦春先生的建议，还是自己另外写了一个简单的。<br/><br/>
编译方法：<br/>
gcc kc.c -fPIC -shared -lkyotocabinet -o kc.so

author
=====
欧远宁 outrace@gmail.com