digraph composite_software {
  /* label = "Possible initialization paths for a component." ; */
  rankdir=LR;
  size = "7" ;
  margin = "0" ;
  overlap = "false" ;
  fontsize = "28" ;
  fontname="Sans serif" ;

  "__cosrt_upcall_entry" [shape=plaintext,fontname="Sans serif"] ;
  "libc + constructors" [shape=plaintext,fontname="Sans serif"] ;
  "cos_init" [shape=ellipse,style=filled,fillcolor=lightblue,fontname="Sans serif"] ;
  "main" [shape=ellipse,style=filled,fillcolor=lightblue,fontname="Sans serif"] ;
  "cos_parallel_init" [shape=ellipse,style=filled,fillcolor=gray,fontname="Sans serif"] ;
  "parallel_main" [shape=ellipse,style=filled,fillcolor=gray,fontname="Sans serif"] ;

  "__cosrt_upcall_entry" -> "libc + constructors" [fontname="Sans serif",style=dashed];
  "libc + constructors" -> "cos_init" [fontname="Sans serif",style=dashed];
  "libc + constructors" -> "cos_parallel_init" [fontname="Sans serif",style=dashed];
  "libc + constructors" -> "main" [fontname="Sans serif",style=solid];
  "libc + constructors" -> "parallel_main" [fontname="Sans serif"style=solid];
  "cos_init" -> "main" [fontname="Sans serif",style=solid];
  "cos_init" -> "cos_parallel_init" [fontname="Sans serif",style=dashed];
  "cos_init" -> "parallel_main" [fontname="Sans serif",style=solid];
  "cos_parallel_init" -> "parallel_main" [fontname="Sans serif",style=solid];
  "cos_parallel_init" -> "main" [fontname="Sans serif",style=solid];
}
