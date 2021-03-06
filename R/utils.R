#' @useDynLib phyr, .registration = TRUE
#' @importFrom Rcpp sourceCpp
NULL

#' @importFrom ape read.tree write.tree drop.tip compute.brlen vcv.phylo vcv is.rooted
#' @importClassesFrom Matrix RsparseMatrix dsCMatrix dgTMatrix
#' @importMethodsFrom Matrix t solve %*% determinant diag crossprod tcrossprod image
#' @importFrom stats as.dendrogram as.dist as.formula binomial dist family fitted 
#'   formula glm lm model.frame make.link model.matrix model.response na.omit 
#'   optim pchisq pnorm printCoefmat reorder reshape residuals rnorm runif sd 
#'   update var poisson
#' @importFrom methods as show is
#' @importFrom graphics par image
NULL

logit <- make.link("logit")$linkfun

inv.logit <- make.link("logit")$linkinv

#' Remove species that not observed in any site.
#'
#' @author Daijiang Li
#'
#' This function will remove species that has no observations in any site.
#'
#' @param df A data frame in wide form, i.e. site by species data frame, with site names as row name.
#' @export
#' @return  A site by species data frame.
rm_sp_noobs = function(df) {
  if (any(colSums(df) == 0)) {
    df = df[, -which(colSums(df) == 0), drop = FALSE]
  }
  df
}

#' Remove site that has no obsrevations of any species.
#'
#' This function will remove site that has no observations in a site by species data frame.
#'
#' @author Daijiang Li
#'
#' @param df A data frame in wide form, i.e. site by species data frame, with site names as row name.
#' @export
#' @return  A site by species data frame.
rm_site_noobs = function(df) {
  if (any(rowSums(df) == 0)) {
    df = df[-which(rowSums(df) == 0), , drop = FALSE]
  }
  df
}

#' Not in
#'
#' This function will return elements of x not in y
#'
#' @param x A vector.
#' @param y A vector.
#' @return A vector.
#' @rdname nin
#' @export
#'
"%nin%" <- function(x, y) {
  return(!(x %in% y))
}

#' Match phylogeny with community data
#'
#' This function will remove species from community data that are not in the phylogeny.
#' It will also remove tips from the phylogeny that are not in the community data.
#'
#' @param comm A site by species data frame, with site names as row names.
#' @param tree A phylogeny with "phylo" as class.
#' @param comm_2 Another optional site by species data frame, if presented, both community data and the phylogeny
#' will have the same set of species. This can be useful for PCD with custom species pool.
#' @return A list of the community data and the phylogeny.
#' @export
#'
match_comm_tree = function(comm, tree, comm_2 = NULL){
  if(class(comm) %nin% c("data.frame", "matrix")){
    stop("Community data needs to be a data frame or a matrix")
  }

  if(!is.null(comm_2) & (class(comm_2) %nin% c("data.frame", "matrix"))){
    stop("Community data needs to be a data frame or a matrix")
  }

  tree_tips = tree$tip.label
  comm_taxa = colnames(comm)
  intersect_taxa = intersect(tree_tips, comm_taxa)
  if(!is.null(comm_2)) intersect_taxa = intersect(intersect_taxa, colnames(comm_2))

  if(length(intersect_taxa) == 0){
    stop("No species in common between the community data and the phylogeny")
  }

  if(!all(tree_tips %in% intersect_taxa)){
    message("Dropping tips from the phylogeny that are not in the community data")
    tree = ape::drop.tip(tree, setdiff(tree_tips, intersect_taxa))
  }

  if(!all(comm_taxa %in% tree_tips)){
    message("Dropping species from the community data that are not in the phylogeny")
  }
  comm = comm[, tree$tip.label] # this will sort the comm data, and remove sp if needed

  if(!is.null(comm_2)){
    return(list(comm = comm, tree = tree, comm_2 = comm_2[, tree$tip.label]))
  } else {
    return(list(comm = comm, tree = tree))
  }
}

#' Create phylogenetic var-cov matrix
#'
#' This function will convert a phylogeny to a Var-cov matrix.
#' 
#' @param phy A phylogeny with "phylo" as class.
#' @param corr Whether to return a correlation matrix instead of Var-cov matrix. Default is FALSE.
#' @return A phylogenetic var-cov matrix.
#' @export
#'
vcv2 = function(phy, corr = FALSE){
  if(corr){
    vcv = ape::vcv.phylo(phy, corr = FALSE)
    cov2cor_cpp(vcv)
    return(vcv)
  } else {
    return(ape::vcv.phylo(phy, corr = FALSE))
  }
}

# # for some reason, this version is not fast as expected.
# vcv2 = function(phy, corr = FALSE){
#   if (is.null(phy$edge.length)) stop("the tree has no branch lengths")
#   pp <- ape::prop.part(phy)
#   phy <- reorder(phy, "postorder")
#   sp = phy$tip.label
#   n <- length(sp)
#   e1 <- phy$edge[, 1]
#   e2 <- phy$edge[, 2]
#   EL <- phy$edge.length
#   xx <- numeric(n + phy$Nnode)
#   vcv = vcv_loop(xx, n, e1, e2, EL, pp, corr)
#   row.names(vcv) = colnames(vcv) = sp
#   vcv
# }


#' Create phylogenetic var-cov matrix based on phylogeny and community data
#'
#' This function will remove species from community data that are not in the phylogeny.
#' It will also remove tips from the phylogeny that are not in the community data. And
#' then convert the phylogeny to a Var-cov matrix.
#'
#' @param comm A site by species data frame, with site names as row names.
#' @param tree A phylogeny with "phylo" as class; or a phylogenetic var-covar matrix.
#' @param prune.tree Whether to prune the tree first then use vcv.phylo function. Default
#' is FALSE: use vcv.phylo first then subsetting the matrix.
#' @param scale.vcv Whether to scale vcv to a correlation matrix.
#' @return A list of the community data and the phylogenetic var-cov matrix.
#' @export
#'
align_comm_V = function(comm, tree, prune.tree = FALSE, scale.vcv = TRUE){
  # remove species and site with no observation
  # comm = rm_site_noobs(rm_sp_noobs(comm))
  # remove species not in the tree
  if (is(tree)[1] == "phylo") {
    comm = comm[, colnames(comm) %in% tree$tip.label, drop = FALSE]
    if (is.null(tree$edge.length)) tree = ape::compute.brlen(tree, 1) # If phylo has no given branch lengths
    if (ape::Ntip(tree) > 5000 | prune.tree) {
      if(prune.tree) warning("Prunning the tree before converting to var-cov matrix may have different results")
      tree = ape::drop.tip(tree, tree$tip.label[tree$tip.label %nin% colnames(comm)])
    }
    # Attention: prune then vcv VS. vcv then subsetting may have different Cmatrix.
    # so, by default, we won't prune the tree unless it is huge
    Cmatrix = vcv2(tree, corr = scale.vcv)  # Make a correlation matrix of the species pool phylogeny
  } else {
    # tree is a matrix
    comm = comm[, colnames(comm) %in% colnames(tree), drop = FALSE]
    Cmatrix = tree
  }
  
  tokeep = which(colnames(Cmatrix) %in% colnames(comm))
  
  Cmatrix = Cmatrix[tokeep, tokeep, drop = FALSE]
  comm = comm[, colnames(Cmatrix), drop = FALSE]
  
  return(list(Cmatrix = Cmatrix, comm = comm))
}

.onLoad <- function(libname, pkgname){
  if (isTRUE(requireNamespace("INLA", quietly = TRUE))) {
    if (!is.element("INLA", (.packages()))) {
      suppressPackageStartupMessages(attachNamespace("INLA"))
    }
  }
  # Below prevents `function 'nlopt_create' not provided by package 'nloptr'`
  # error in `cor_phylo` when using bobyqa optimizer
  z <- nloptr::bobyqa(0, function(.x) .x^2, 
                      control = list(maxeval = 10))
  invisible()
}



#' Check phylogeny and reorder it.
#' 
#' It checks for it being `phylo` class, having branch lengths, and having tip labels.
#'
#' @param phy A phylogeny that should be a `phylo` object.
#'
#' @return A phylogenetic tree that's been reordered using `ape::reorder.phylo(phy, "postorder")`
#'
#' @noRd
#' 
check_phy <- function(phy) {
  
  if (!inherits(phy, "phylo")) {
    stop("\nThe input phylogeny is not of class \"phylo\".")
  }
  if (is.null(phy$edge.length)) {
    stop("\nThe input phylogeny has no branch lengths.")
  }
  if (is.null(phy$tip.label)) {
    stop("\nThe input phylogeny has no tip labels.")
  }
  
  phy <- ape::reorder.phylo(phy, "postorder")
}




#' Retrieve an argument value based on a function call.
#' 
#' If not present in the call, this returns the default value for that function.
#' Note that this returns an object of class `call`, not a vector.
#' Note also that if you used `match.arg` inside the function, you should do
#' `eval(call_arg(...))[1]` to get the actual value of the argument used.
#' 
#'
#' @param .call Call to a function.
#' @param .arg Name of argument from the function.
#' 
#' @noRd
#' 
call_arg <- function(.call, .arg) {
  
  .fun <- eval(.call[[1]])
  fun_formals <- formals(.fun)
  default_value <- fun_formals[[.arg]]
  
  call_list <- as.list(.call)[-1]
  if (is.null(call_list[[.arg]])) {
    if (is.null(names(call_list))) {
      if (length(call_list) < which(names(fun_formals) == .arg)) return(default_value)
      names(call_list) <- rep("", length(call_list))
    }
    
    # arguments in `.fun` not represented by names in the call:
    cp_args <- fun_formals[!names(fun_formals) %in% names(call_list)]
    
    # removing named arguments from `.fun` call bc we already know they don't
    # contain `.arg`
    call_list <- call_list[names(call_list) == ""]
    
    if (length(call_list) < which(names(cp_args) == .arg)) {
      return(default_value)
    } else {
      return(cp_args[[which(names(cp_args) == .arg)]])
    }
    
  } else {
    return(call_list[[.arg]])
  }
}


#' Generic method to output bootstrap confidence intervals from an object.
#'
#' Implemented only for `cor_phylo` objects thus far.
#'
#' @param mod A `cor_phylo` object.
#' @param ... Additional arguments.
#' @export
#'
boot_ci <- function(mod, ...) UseMethod("boot_ci")

