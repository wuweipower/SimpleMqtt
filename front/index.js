$(document).ready(function(){
$("#btn").click(function(){
    console.log("hello")
    $.ajax({
        dataType: "JOSNP",
        type: "post",
        url: "http://localhost:8089/send",
        data: {
            topic: $("#topic").val(),
            content: $("#content").val(),
        },
        crossDomain:true,
        success: function(res)
        {
            console.log(res)
        },
        error: function()
        {
            console.error("error!")
        }     
    })
})
});



